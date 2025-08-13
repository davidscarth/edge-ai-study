/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025 davidscarth
 */

#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

#ifndef VK_AT_COMPILE_TOOL
#define VK_AT_COMPILE_TOOL "unknown"
#endif

#define VK_CHECK(x) do { VkResult err = (x); if (err) { fprintf(stderr,"Vulkan error %d at %s:%d\n", err, __FILE__, __LINE__); std::exit(1);} } while(0)

static std::vector<uint32_t> load_spirv(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror("fopen gemm.spv"); std::exit(1); }
    fseek(f,0,SEEK_END); long sz = ftell(f); fseek(f,0,SEEK_SET);
    std::vector<uint32_t> buf((sz+3)/4);
    size_t rd = fread(buf.data(),1,sz,f); (void)rd; fclose(f);
    return buf;
}

static uint32_t ceil_div(uint32_t a, uint32_t b){ return (a + b - 1u)/b; }

struct VulkanCtx {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice pdev = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t qfam = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceSubgroupProperties subprops{};
    VkCommandPool cpool = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout ppl = VK_NULL_HANDLE;
    VkDescriptorPool dpool = VK_NULL_HANDLE;
    VkQueryPool qpool = VK_NULL_HANDLE;
    VkBuffer bufA = VK_NULL_HANDLE, bufB = VK_NULL_HANDLE, bufC = VK_NULL_HANDLE;
    VkDeviceMemory memA = VK_NULL_HANDLE, memB = VK_NULL_HANDLE, memC = VK_NULL_HANDLE;
    double timestamp_period_ns = 1.0;
};

static void create_buffer(VulkanCtx& C, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(C.device, &bi, nullptr, &buf));

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(C.device, buf, &mr);

    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(C.pdev, &mp);
    uint32_t idx = UINT32_MAX;
    for (uint32_t i=0;i<mp.memoryTypeCount;i++){
        if ((mr.memoryTypeBits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { idx=i; break; }
    }
    if (idx==UINT32_MAX) { fprintf(stderr,"No HOST_VISIBLE|HOST_COHERENT memory found\n"); std::exit(1); }

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = idx;
    VK_CHECK(vkAllocateMemory(C.device, &ai, nullptr, &mem));
    VK_CHECK(vkBindBufferMemory(C.device, buf, mem, 0));
}

static bool contains_str(const char* s, const char* needle){
    return s && needle && std::strstr(s, needle);
}

static void init_vulkan(VulkanCtx& C) {
    // Instance
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "vk-autotune";
    ai.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    VK_CHECK(vkCreateInstance(&ici, nullptr, &C.instance));

    // Pick compute queue physical device (avoid software stacks)
    uint32_t ndev=0; VK_CHECK(vkEnumeratePhysicalDevices(C.instance, &ndev, nullptr));
    std::vector<VkPhysicalDevice> devs(ndev); VK_CHECK(vkEnumeratePhysicalDevices(C.instance, &ndev, devs.data()));

    for (auto d : devs) {
        VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(d, &p);
        uint32_t qf=0; vkGetPhysicalDeviceQueueFamilyProperties(d, &qf, nullptr);
        std::vector<VkQueueFamilyProperties> qfp(qf); vkGetPhysicalDeviceQueueFamilyProperties(d, &qf, qfp.data());
        int computeFam = -1;
        for (uint32_t i=0;i<qf;i++) if (qfp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { computeFam = (int)i; break; }
        if (computeFam<0) continue;
        if (contains_str(p.deviceName, "llvmpipe") || contains_str(p.deviceName, "lavapipe") || contains_str(p.deviceName, "software")) continue;
        C.pdev = d; C.qfam = (uint32_t)computeFam; C.props = p; break;
    }
    if (!C.pdev) { // fallback
        C.pdev = devs[0];
        vkGetPhysicalDeviceProperties(C.pdev, &C.props);
        uint32_t qf=0; vkGetPhysicalDeviceQueueFamilyProperties(C.pdev, &qf, nullptr);
        std::vector<VkQueueFamilyProperties> qfp(qf); vkGetPhysicalDeviceQueueFamilyProperties(C.pdev, &qf, qfp.data());
        for (uint32_t i=0;i<qf;i++) if (qfp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { C.qfam = i; break; }
    }

    // Subgroup props
    C.subprops = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    VkPhysicalDeviceProperties2 p2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    p2.pNext = &C.subprops;
    vkGetPhysicalDeviceProperties2(C.pdev, &p2);

    // Device
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = C.qfam;
    qci.queueCount = 1; qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    VK_CHECK(vkCreateDevice(C.pdev, &dci, nullptr, &C.device));
    vkGetDeviceQueue(C.device, C.qfam, 0, &C.queue);

    // Command pool
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = C.qfam;
    VK_CHECK(vkCreateCommandPool(C.device, &pci, nullptr, &C.cpool));

    // Descriptor set layout (A,B,C)
    VkDescriptorSetLayoutBinding b[3] = {};
    for (int i=0;i<3;i++){ b[i].binding = i; b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; b[i].descriptorCount=1; b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; }
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 3; dlci.pBindings = b;
    VK_CHECK(vkCreateDescriptorSetLayout(C.device, &dlci, nullptr, &C.dsl));

    // Pipeline layout (push constants)
    VkPushConstantRange pcr{}; pcr.offset=0; pcr.size=6*sizeof(uint32_t); pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &C.dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(C.device, &plci, nullptr, &C.ppl));

    // Descriptor pool
    VkDescriptorPoolSize dps{}; dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; dps.descriptorCount = 3;
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &dps;
    VK_CHECK(vkCreateDescriptorPool(C.device, &dpci, nullptr, &C.dpool));

    // Query pool (timestamps)
    VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP; qpci.queryCount = 2;
    VK_CHECK(vkCreateQueryPool(C.device, &qpci, nullptr, &C.qpool));

    C.timestamp_period_ns = C.props.limits.timestampPeriod ? C.props.limits.timestampPeriod : 1.0;

    // Banner
    fprintf(stderr,
        "# Device: %s (API %u.%u)  driver=%u\n"
        "# maxWGInvocations=%u, maxSharedMemPerWG=%u bytes, subgroupSize=%u\n"
        "# shader-compiler=%s\n",
        C.props.deviceName,
        VK_VERSION_MAJOR(C.props.apiVersion), VK_VERSION_MINOR(C.props.apiVersion),
        C.props.driverVersion,
        C.props.limits.maxComputeWorkGroupInvocations,
        C.props.limits.maxComputeSharedMemorySize,
        C.subprops.subgroupSize,
        VK_AT_COMPILE_TOOL
    );
}

struct Cand { uint32_t TM,TN,TK, lszx, lszy, smem; };

static std::vector<std::pair<uint32_t,uint32_t>> parse_lsz(const char* s) {
    if (!s || !*s) return {{16,8},{16,4},{16,1}};
    std::vector<std::pair<uint32_t,uint32_t>> out; uint32_t a=0,b=0; bool ia=false,ib=false,ix=false;
    for (const char* p=s;;++p){ char c=*p;
        if (c>='0'&&c<='9'){ (ix? b:a) = (ix? b:a)*10 + (c-'0'); (ix? ib:ia)=true; }
        else if (c=='x'||c=='X'){ ix=true; }
        else { if (ia&&ib) out.emplace_back(a,b); a=b=0; ia=ib=ix=false; if(!c) break; }
    }
    if (out.empty()) out = {{16,8},{16,4},{16,1}};
    return out;
}
static std::vector<uint32_t> parse_u32_list(const char* s, std::initializer_list<uint32_t> def) {
    if (!s || !*s) return std::vector<uint32_t>(def);
    std::vector<uint32_t> out; uint64_t v=0; bool in=false;
    for (const char* p=s;;++p){ char c=*p;
        if (c>='0'&&c<='9'){ v=v*10+(c-'0'); in=true; }
        else { if(in){ out.push_back((uint32_t)v); v=0; in=false; } if(!c) break; }
    }
    if (out.empty()) out.assign(def.begin(), def.end());
    return out;
}
static std::vector<uint32_t> parse_u32_list(const char* s, const std::vector<uint32_t>& def) {
    if (!s || !*s) return def;
    std::vector<uint32_t> out; uint64_t v=0; bool in=false;
    for (const char* p=s;;++p){ char c=*p;
        if (c>='0'&&c<='9'){ v=v*10+(c-'0'); in=true; }
        else { if(in){ out.push_back((uint32_t)v); v=0; in=false; } if(!c) break; }
    }
    if (out.empty()) return def;
    return out;
}

static void add_tile(std::vector<Cand>& G, uint32_t TM, uint32_t TN, uint32_t TK,
                     const std::vector<std::pair<uint32_t,uint32_t>>& LSZ, bool smem,
                     uint32_t maxWGInv, uint32_t maxSMEM,
                     uint32_t maxWGSizeX, uint32_t maxWGSizeY,
                     int max_rn, int max_rm) {
    for (auto [x,y] : LSZ) {
        if (x*y > maxWGInv) continue;
        if (x > maxWGSizeX || y > maxWGSizeY) continue;
        if (max_rn > 0) { uint32_t RN = (TN + x - 1) / x; if ((int)RN > max_rn) continue; }
        if (max_rm > 0) { uint32_t RM = (TM + y - 1) / y; if ((int)RM > max_rm) continue; }
        if (smem) {
            uint64_t bytes = 4ull*TK*(TM + TN);
            if (bytes > maxSMEM) continue;
        }
        G.push_back({TM,TN,TK,x,y,(uint32_t)smem});
    }
}

static std::vector<Cand> build_grid(const VkPhysicalDeviceProperties& props, uint32_t subgroup,
                                    int argc, char** argv) {
    std::vector<Cand> grid;
    // defaults
    std::string preset = "classic"; // classic | classic_legacy | extended16k | extended16k_capped
    std::vector<std::pair<uint32_t,uint32_t>> LSZ;
    std::vector<uint32_t> Ms = {64,80,96,112};
    std::vector<uint32_t> Ns = {32,48,64,80};
    bool enable_smem = true, enable_nosmem = false;
    int  max_rn = -1;
    int  max_rm = -1;
    std::vector<std::pair<uint32_t,uint32_t>> add_pairs;

    // parse argv
    for (int i=1;i<argc;i++){
        const char* a = argv[i];
        if (!strncmp(a,"--preset=",9)) preset = a+9;
        else if (!strncmp(a,"--lsz=",6))  LSZ = parse_lsz(a+6);
        else if (!strncmp(a,"--Ms=",5))   Ms  = parse_u32_list(a+5, Ms);
        else if (!strncmp(a,"--Ns=",5))   Ns  = parse_u32_list(a+5, Ns);
        else if (!strncmp(a,"--enable-smem=",14))    enable_smem   = atoi(a+14)!=0;
        else if (!strncmp(a,"--enable-nosmem=",16))  enable_nosmem = atoi(a+16)!=0;
        else if (!strncmp(a,"--max-rn=",9))          max_rn = atoi(a+9);
        else if (!strncmp(a,"--max-rm=",9))          max_rm = atoi(a+9);
        else if (!strncmp(a,"--add-tiles=",12)) {
            const char* s = a+12; uint32_t tm=0,tn=0; bool got_tm=false;
            for (const char* p=s;;++p){ char c=*p;
                if (c>='0'&&c<='9'){ (got_tm? tn:tm) = (got_tm? tn:tm)*10 + (c-'0'); }
                else if (c=='x'||c=='X'){ got_tm=true; }
                else { if (tm&&tn) add_pairs.emplace_back(tm,tn); tm=tn=0; got_tm=false; if(!c) break; }
            }
        }
    }
    // env overrides
    if (const char* e=getenv("AT_PRESET")) preset = e;
    if (const char* e=getenv("AT_LSZ"))    LSZ = parse_lsz(e);
    if (const char* e=getenv("AT_MS"))     Ms  = parse_u32_list(e, Ms);
    if (const char* e=getenv("AT_NS"))     Ns  = parse_u32_list(e, Ns);
    if (const char* e=getenv("AT_ENABLE_SMEM"))   enable_smem   = atoi(e)!=0;
    if (const char* e=getenv("AT_ENABLE_NOSMEM")) enable_nosmem = atoi(e)!=0;
    if (const char* e=getenv("AT_MAX_RN"))        max_rn        = atoi(e);
    if (const char* e=getenv("AT_MAX_RM"))        max_rm        = atoi(e);

    // default lanes per preset (if not specified)
    if (LSZ.empty()) {
        if (preset=="classic")                 LSZ = {{16,8},{16,4}};
        else if (preset=="classic_legacy")     LSZ = {{16,8},{16,4},{16,1}};
        else if (preset=="extended16k")        LSZ = {{16,8},{16,4},{16,1}};
        else if (preset=="extended16k_capped") LSZ = {{16,8},{16,16}};
        else LSZ = {{16,8},{16,4},{16,1}};
    }

    // Default per-thread microtile caps to 8x8 unless user overrides
    if (max_rn <= 0) max_rn = 8;
    if (max_rm <= 0) max_rm = 8;

    uint32_t TK = subgroup;
    uint32_t maxWGInv = props.limits.maxComputeWorkGroupInvocations;
    uint32_t maxSMEM  = props.limits.maxComputeSharedMemorySize;
    uint32_t maxWGSizeX = props.limits.maxComputeWorkGroupSize[0];
    uint32_t maxWGSizeY = props.limits.maxComputeWorkGroupSize[1];

    auto add_default_4x4 = [&](){
        if (!enable_smem) return;
        for (uint32_t TM : Ms) for (uint32_t TN : Ns) {
            if (4ull*TK*(TM + TN) > maxSMEM) continue;
            add_tile(grid, TM,TN,TK, LSZ, /*smem=*/true, maxWGInv, maxSMEM, maxWGSizeX, maxWGSizeY, max_rn, max_rm);
        }
    };
    auto add_baselines = [&](){
        if (!enable_nosmem) return;
        add_tile(grid, 32,32,TK, LSZ, /*smem=*/false, maxWGInv, maxSMEM, maxWGSizeX, maxWGSizeY, max_rn, max_rm);
    };

    if (preset=="classic" || preset=="classic_legacy") {
        add_default_4x4();
        add_baselines();
        if (preset=="classic") {
            const std::pair<uint32_t,uint32_t> top10[] = {
                {96,64},{112,64},{128,64},{144,64},{160,64},{176,64},{192,64},
                {128,48},
                {64,128},
                {32,64}
            };
            for (auto [TM,TN] : top10){
                if (4ull*TK*(TM + TN) > maxSMEM) continue;
                add_tile(grid, TM,TN,TK, LSZ, true, maxWGInv, maxSMEM, maxWGSizeX, maxWGSizeY, max_rn, max_rm);
            }
        }
    } else if (preset=="extended16k" || preset=="extended16k_capped") {
        std::vector<uint32_t> Ms2 = {128,144,160,176,192};
        Ms.insert(Ms.end(), Ms2.begin(), Ms2.end());
        std::vector<uint32_t> Ns2 = (preset=="extended16k_capped")
            ? std::vector<uint32_t>{32,48,64,80}
            : std::vector<uint32_t>{32,48,64,80,96,112,128};
        Ns.insert(Ns.end(), Ns2.begin(), Ns2.end());
        std::sort(Ms.begin(),Ms.end()); Ms.erase(std::unique(Ms.begin(),Ms.end()),Ms.end());
        std::sort(Ns.begin(),Ns.end()); Ns.erase(std::unique(Ns.begin(),Ns.end()),Ns.end());

        if (enable_smem) {
            for (uint32_t TM : Ms) for (uint32_t TN : Ns) {
                if (4ull*TK*(TM + TN) > maxSMEM) continue;
                add_tile(grid, TM,TN,TK, LSZ, true, maxWGInv, maxSMEM, maxWGSizeX, maxWGSizeY, max_rn, max_rm);
            }
        }
        add_baselines();
    }

    if (!add_pairs.empty()) {
        for (auto [TM,TN] : add_pairs) {
            if (4ull*TK*(TM + TN) > maxSMEM) continue;
            add_tile(grid, TM,TN,TK, LSZ, true, maxWGInv, maxSMEM, maxWGSizeX, maxWGSizeY, max_rn, max_rm);
        }
    }

    std::sort(grid.begin(), grid.end(), [](const Cand&a,const Cand&b){
        if (a.smem!=b.smem) return a.smem>b.smem;
        if (a.TM!=b.TM) return a.TM<b.TM;
        if (a.TN!=b.TN) return a.TN<b.TN;
        if (a.TK!=b.TK) return a.TK<b.TK;
        if (a.lszx!=b.lszx) return a.lszx<b.lszx;
        return a.lszy<b.lszy;
    });
    grid.erase(std::unique(grid.begin(), grid.end(), [](const Cand&a,const Cand&b){
        return a.TM==b.TM && a.TN==b.TN && a.TK==b.TK &&
               a.lszx==b.lszx && a.lszy==b.lszy && a.smem==b.smem;
    }), grid.end());

    fprintf(stderr, "# Preset=%s  lanes=", preset.c_str());
    for (size_t i=0;i<LSZ.size();++i){ fprintf(stderr, "%ux%u%s", LSZ[i].first, LSZ[i].second, (i+1<LSZ.size())?",":""); }
    fprintf(stderr, "  candidates=%zu\n", grid.size());
    return grid;
}

struct RunCfg {
    uint32_t M=1024, N=1024, K=1024;
    uint32_t WARM=5, REP=30;
    uint64_t TIMEOUT_MS=600000; // per-candidate
    double   SMEM_FRAC=1.0;
    const char* CSV=nullptr;
};

static RunCfg env_runcfg() {
    RunCfg r;
    if (const char* s=getenv("AT_M")) r.M=std::atoi(s);
    if (const char* s=getenv("AT_N")) r.N=std::atoi(s);
    if (const char* s=getenv("AT_K")) r.K=std::atoi(s);
    if (const char* s=getenv("AT_WARM")) r.WARM=std::atoi(s);
    if (const char* s=getenv("AT_REP")) r.REP=std::atoi(s);
    if (const char* s=getenv("AT_TIMEOUT_MS")) r.TIMEOUT_MS=std::strtoull(s,nullptr,10);
    if (const char* s=getenv("AT_CSV")) r.CSV=s;
    if (const char* s=getenv("AT_SMEM_FRAC")) r.SMEM_FRAC=std::max(0.5, std::min(1.0, atof(s)));
    return r;
}

static VkShaderModule make_shader(VkDevice dev, const std::vector<uint32_t>& spv){
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spv.size()*sizeof(uint32_t);
    ci.pCode = spv.data();
    VkShaderModule mod;
    VK_CHECK(vkCreateShaderModule(dev, &ci, nullptr, &mod));
    return mod;
}

int main(int argc, char** argv){
    VulkanCtx C; init_vulkan(C);
    auto cfg = env_runcfg();

    // Allocate buffers (FP32)
    size_t sizeA = (size_t)cfg.M * cfg.K * sizeof(float);
    size_t sizeB = (size_t)cfg.K * cfg.N * sizeof(float);
    size_t sizeC = (size_t)cfg.M * cfg.N * sizeof(float);
    create_buffer(C, sizeA, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, C.bufA, C.memA);
    create_buffer(C, sizeB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, C.bufB, C.memB);
    create_buffer(C, sizeC, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, C.bufC, C.memC);

    // Fill A,B with 1.0f
    void* p;
    vkMapMemory(C.device, C.memA, 0, VK_WHOLE_SIZE, 0, &p); std::fill_n((float*)p, (sizeA/4), 1.0f); vkUnmapMemory(C.device, C.memA);
    vkMapMemory(C.device, C.memB, 0, VK_WHOLE_SIZE, 0, &p); std::fill_n((float*)p, (sizeB/4), 1.0f); vkUnmapMemory(C.device, C.memB);

    // Descriptor set
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = C.dpool; dsai.descriptorSetCount=1; dsai.pSetLayouts=&C.dsl;
    VkDescriptorSet dset; VK_CHECK(vkAllocateDescriptorSets(C.device, &dsai, &dset));

    VkDescriptorBufferInfo biA{C.bufA,0,sizeA}, biB{C.bufB,0,sizeB}, biC{C.bufC,0,sizeC};
    VkWriteDescriptorSet w[3]{};
    for (int i=0;i<3;i++){ w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet=dset; w[i].dstBinding=i; w[i].descriptorCount=1; w[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; }
    w[0].pBufferInfo=&biA; w[1].pBufferInfo=&biB; w[2].pBufferInfo=&biC;
    vkUpdateDescriptorSets(C.device, 3, w, 0, nullptr);

    // Load shader
    std::string spv_path = std::string("shaders/gemm.spv"); // in build dir
    auto spv = load_spirv(spv_path.c_str());
    VkShaderModule mod = make_shader(C.device, spv);

    // Build candidate grid
    auto grid = build_grid(C.props, C.subprops.subgroupSize, argc, argv);

    // CSV header
    FILE* csv = nullptr;
    if (cfg.CSV) {
        csv = fopen(cfg.CSV, "w");
        if (csv) fprintf(csv, "TM,TN,TK,lszx,lszy,smem,M,N,K,WARM,REP,status,usec_per_iter,gflops\n");
    }

    uint32_t idx=0;
    for (const auto& g : grid) {
        idx++;
        printf("[%u/%zu] TM=%u TN=%u TK=%u lsz=(%u,%u) smem=%u  ...\n",
            idx, grid.size(), g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem);
        fflush(stdout);

        // SMEM budget check with optional safety fraction
        uint32_t budget = (uint32_t)(C.props.limits.maxComputeSharedMemorySize * std::min(std::max(cfg.SMEM_FRAC,0.5),1.0));
        uint32_t needed_bytes = 4u * g.TK * (g.TM + g.TN);
        if (g.smem && needed_bytes > budget) {
            fprintf(stdout, "  -> [SKIP] needs %uB > budget %uB\n", needed_bytes, budget);
            if (csv) fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,SKIP_SMEM_BUDGET,0.0,0.0\n",
                g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem,cfg.M,cfg.N,cfg.K,cfg.WARM,cfg.REP);
            continue;
        }

        // Spec constants: 0->LSX,1->LSY, 2->TM,3->TN,4->TK,5->USE_SMEM,6->SH_ELEMS
        uint32_t SH_ELEMS = g.TM*g.TK + g.TK*g.TN;
        struct Spec { uint32_t lsx, lsy, TM, TN, TK, USE_SMEM, SH_ELEMS; }
            spec = { g.lszx, g.lszy, g.TM, g.TN, g.TK, g.smem, SH_ELEMS };
        VkSpecializationMapEntry me[7];
        for (uint32_t i=0;i<7;i++){ me[i].constantID=i; me[i].offset=i*sizeof(uint32_t); me[i].size=sizeof(uint32_t); }
        VkSpecializationInfo si{}; si.mapEntryCount=7; si.pMapEntries=me; si.dataSize=sizeof(Spec); si.pData=&spec;

        VkPipelineShaderStageCreateInfo ss{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        ss.stage = VK_SHADER_STAGE_COMPUTE_BIT; ss.module = mod; ss.pName = "main"; ss.pSpecializationInfo = &si;

        VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pci.stage = ss; pci.layout = C.ppl;
        VkPipeline pipe;
        VkResult perr = vkCreateComputePipelines(C.device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipe);
        if (perr != VK_SUCCESS) {
            fprintf(stdout, "  -> [COMPILE_FAIL]\n");
            if (csv) fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,COMPILE_FAIL,0.0,0.0\n",
                g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem,cfg.M,cfg.N,cfg.K,cfg.WARM,cfg.REP);
            continue;
        }

        // Command buffer
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.commandPool = C.cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VkCommandBuffer cb; VK_CHECK(vkAllocateCommandBuffers(C.device, &cbai, &cb));

        VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CHECK(vkBeginCommandBuffer(cb, &cbi));

        // Reset timestamps
        vkCmdResetQueryPool(cb, C.qpool, 0, 2);

        // Bind pipeline + descriptors
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, C.ppl, 0, 1, &dset, 0, nullptr);

        // Push constants
        struct Push { uint32_t M,N,K,lda,ldb,ldc; } push = { cfg.M, cfg.N, cfg.K, cfg.K, cfg.N, cfg.N };
        vkCmdPushConstants(cb, C.ppl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);

        uint32_t groupsX = ceil_div(cfg.N, g.TN);
        uint32_t groupsY = ceil_div(cfg.M, g.TM);

        // warmup
        for (uint32_t i=0;i<cfg.WARM;i++) {
            vkCmdDispatch(cb, groupsX, groupsY, 1);
        }

        // timed reps
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, C.qpool, 0);
        for (uint32_t i=0;i<cfg.REP;i++) { vkCmdDispatch(cb, groupsX, groupsY, 1); }
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, C.qpool, 1);

        VK_CHECK(vkEndCommandBuffer(cb));

        // Submit & wait with timeout
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence; VK_CHECK(vkCreateFence(C.device, &fci, nullptr, &fence));
        VkSubmitInfo si2{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si2.commandBufferCount = 1; si2.pCommandBuffers = &cb;
        VK_CHECK(vkQueueSubmit(C.queue, 1, &si2, fence));
        VkResult wres = vkWaitForFences(C.device, 1, &fence, VK_TRUE, cfg.TIMEOUT_MS*1000000ull);
        if (wres == VK_TIMEOUT) {
            fprintf(stdout, "  -> [TIMEOUT] after %llu ms (skipping result)\n", (unsigned long long)cfg.TIMEOUT_MS);
            if (csv) fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,TIMEOUT,0.0,0.0\n",
                g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem,cfg.M,cfg.N,cfg.K,cfg.WARM,cfg.REP);
            vkDestroyFence(C.device, fence, nullptr);
            vkFreeCommandBuffers(C.device, C.cpool, 1, &cb);
            vkDestroyPipeline(C.device, pipe, nullptr);
            continue;
        } else if (wres != VK_SUCCESS) {
            fprintf(stdout, "  -> [WAIT_FAIL] err=%d\n", wres);
            if (csv) fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,WAIT_FAIL,0.0,0.0\n",
                g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem,cfg.M,cfg.N,cfg.K,cfg.WARM,cfg.REP);
            vkDestroyFence(C.device, fence, nullptr);
            vkFreeCommandBuffers(C.device, C.cpool, 1, &cb);
            vkDestroyPipeline(C.device, pipe, nullptr);
            continue;
        }

        // Read timestamps
        uint64_t t[2]={0,0};
        VK_CHECK(vkGetQueryPoolResults(C.device, C.qpool, 0, 2, sizeof(t), t, sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

        double elapsed_ns = double(t[1]-t[0]) * C.timestamp_period_ns;
        double usec_per_iter = (elapsed_ns / 1000.0) / double(cfg.REP);

        // GFLOPs = (2*M*N*K) / time (us->s)
        double flops = 2.0 * double(cfg.M) * double(cfg.N) * double(cfg.K);
        double gflops = flops / (usec_per_iter * 1e3);

        printf("  -> [OK] TM=%u TN=%u TK=%u lsz=(%u,%u) smem=%u  usec=%.3f  GFLOP/s=%.6f\n",
            g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem, usec_per_iter, gflops);
        if (csv) fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,OK,%.6f,%.6f\n",
            g.TM,g.TN,g.TK,g.lszx,g.lszy,g.smem,cfg.M,cfg.N,cfg.K,cfg.WARM,cfg.REP, usec_per_iter, gflops);

        // cleanup command resources
        vkDestroyFence(C.device, fence, nullptr);
        vkFreeCommandBuffers(C.device, C.cpool, 1, &cb);
        vkDestroyPipeline(C.device, pipe, nullptr);
    }

    if (csv) fclose(csv);

    // Cleanup
    vkDestroyShaderModule(C.device, mod, nullptr);
    vkDestroyBuffer(C.device, C.bufA, nullptr);
    vkDestroyBuffer(C.device, C.bufB, nullptr);
    vkDestroyBuffer(C.device, C.bufC, nullptr);
    vkFreeMemory(C.device, C.memA, nullptr);
    vkFreeMemory(C.device, C.memB, nullptr);
    vkFreeMemory(C.device, C.memC, nullptr);
    vkDestroyDescriptorPool(C.device, C.dpool, nullptr);
    vkDestroyPipelineLayout(C.device, C.ppl, nullptr);
    vkDestroyDescriptorSetLayout(C.device, C.dsl, nullptr);
    vkDestroyQueryPool(C.device, C.qpool, nullptr);
    vkDestroyCommandPool(C.device, C.cpool, nullptr);
    vkDestroyDevice(C.device, nullptr);
    vkDestroyInstance(C.instance, nullptr);
    return 0;
}
