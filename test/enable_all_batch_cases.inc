struct AllBatchFake:Fake {
    std::map<uintptr_t,void*> originals;
    std::vector<std::vector<uintptr_t>> cohorts;
    unsigned fail_batch=0;
    bool fail_resume=false;
    MT2FW_MT2_RoutesV2 all_api(){auto value=api2();value.hook_enable_batch=all_batch;return value;}
    static MT2FW_MT2_Result MT2FW_CALL all_batch(void* context,const char* const* names,uint32_t count,MT2FW_MT2_RouteReceipt* out,uint32_t capacity){
        auto& self=*static_cast<AllBatchFake*>(context);self.calls.push_back("batch-all");
        CHECK(count>=1&&count<=128&&count==capacity);std::vector<uintptr_t> targets;
        const bool failed=self.fail_batch==self.cohorts.size()+1;
        for(uint32_t i=0;i<count;++i){auto& value=self.routes.at(names[i]);
            CHECK(out[i].size==sizeof(out[i])&&value.state==MT2FW_MT2_ROUTE_PREPARED);
            CHECK(self.originals.at(value.entry)==reinterpret_cast<void*>(value.trampoline));
            CHECK(i==0||targets.back()<value.entry);targets.push_back(value.entry);
            value.state=failed?MT2FW_MT2_ROUTE_UNKNOWN:MT2FW_MT2_ROUTE_ENABLED;
            value.current[0]=0xe9;if(failed)copy(value.cause,"all cohort failed after activation");out[i]=value;
        }
        self.cohorts.push_back(std::move(targets));return failed?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL retained_disable(void* context,const char* name,MT2FW_MT2_RouteReceipt* out){
        auto& self=*static_cast<AllBatchFake*>(context);self.calls.push_back(std::string("disable:")+name);
        auto& value=self.routes.at(name);value.state=MT2FW_MT2_ROUTE_DISABLED_RETAINED;*out=value;return MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL retained_resume(void* context,const char* name,MT2FW_MT2_RouteReceipt* out){
        auto& self=*static_cast<AllBatchFake*>(context);self.calls.push_back(std::string("resume:")+name);
        auto& value=self.routes.at(name);CHECK(value.state==MT2FW_MT2_ROUTE_DISABLED_RETAINED);
        value.state=self.fail_resume?MT2FW_MT2_ROUTE_UNKNOWN:MT2FW_MT2_ROUTE_ENABLED;
        if(self.fail_resume)copy(value.cause,"retained resume failed");*out=value;return self.fail_resume?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL unused_free(void*,MT2FW_MT2_FreeReceipt*){CHECK(false);return MT2FW_MT2_FAILED;}
    static MT2FW_MT2_Result MT2FW_CALL unused_prepare(void*,MT2FW_MT2_FreeFn,MT2FW_MT2_FreeReceipt*){CHECK(false);return MT2FW_MT2_FAILED;}
    MT2FW_MT2_OwnershipV1 ownership(){MT2FW_MT2_OwnershipV1 value{};value.size=sizeof(value);value.version=MT2FW_MT2_OWNERSHIP_VERSION;
        value.context=this;value.hook_disable=retained_disable;value.hook_resume=retained_resume;value.free_prepare=unused_prepare;
        value.free_enable=value.free_disable=value.free_read=value.free_remove=unused_free;return value;}
    void prepare(Backend& backend,const Target& target){void* original=nullptr;
        CHECK(backend.create(address(target.rva),detour(),&original)==MH_OK);originals.emplace(image+target.rva,original);}
};
void enable_all_batch_cases(){
    std::vector<Target> rows;for(const auto& target:catalog)if(!target.shared)rows.push_back(target);
    std::sort(rows.begin(),rows.end(),[](const auto& left,const auto& right){return left.rva<right.rva;});CHECK(rows.size()>129);
    for(const size_t count:{size_t(2),size_t(128),size_t(129)}){
        AllBatchFake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.all_api()));
        for(size_t i=0;i<count;++i)fake.prepare(backend,rows[i]);fake.calls.clear();
        CHECK(backend.enable(MH_ALL_HOOKS)==MH_OK);CHECK(fake.cohorts.size()==(count+127)/128&&fake.count("enable:")==0);
        CHECK(fake.cohorts[0].size()==(std::min)(count,size_t(128)));if(count==129)CHECK(fake.cohorts[1].size()==1);
        fake.calls.clear();CHECK(backend.enable(MH_ALL_HOOKS)==MH_OK);
        CHECK(fake.count("read:")==count&&fake.count("batch-all")==0&&fake.count("enable:")==0);
    }
    for(unsigned mode=0;mode<3;++mode){
        AllBatchFake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.all_api()));CHECK(backend.bind_ownership(fake.ownership()));
        for(size_t i=0;i<5;++i)fake.prepare(backend,rows[i]);
        CHECK(backend.enable(address(rows[1].rva))==MH_OK);CHECK(backend.enable(address(rows[2].rva))==MH_OK);
        CHECK(backend.disable(address(rows[2].rva))==MH_OK);CHECK(backend.remove(address(rows[3].rva))==MH_OK);
        if(mode==1)++fake.routes.at(rows[1].name).generation;if(mode==2)fake.fail_resume=true;fake.calls.clear();
        CHECK((backend.enable(MH_ALL_HOOKS)==MH_OK)==(mode==0));CHECK(fake.count("enable:")==0);
        CHECK(fake.cohorts.size()==(mode==0?2u:1u));CHECK(fake.cohorts[0]==std::vector<uintptr_t>{image+rows[0].rva});
        std::vector<std::string> expected{std::string("read:")+rows[0].name,"batch-all",std::string("read:")+rows[1].name};
        if(mode!=1){expected.push_back(std::string("read:")+rows[2].name);expected.push_back(std::string("resume:")+rows[2].name);}
        if(mode==0){expected.push_back(std::string("read:")+rows[4].name);expected.push_back("batch-all");}
        CHECK(fake.calls==expected);CHECK(fake.routes.at(rows[3].name).state==MT2FW_MT2_ROUTE_REMOVED);
        CHECK(fake.routes.at(rows[4].name).state==uint32_t(mode==0?MT2FW_MT2_ROUTE_ENABLED:MT2FW_MT2_ROUTE_PREPARED));
    }
    {AllBatchFake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.all_api()));
        fake.prepare(backend,rows[0]);fake.fail_prepare=true;void* untouched=nullptr;
        CHECK(backend.create(address(rows[1].rva),detour(),&untouched)==MH_UNKNOWN);fake.fail_prepare=false;fake.prepare(backend,rows[2]);fake.calls.clear();
        CHECK(backend.enable(MH_ALL_HOOKS)==MH_UNKNOWN);CHECK(fake.cohorts.size()==1&&fake.cohorts[0].size()==1);
        CHECK(fake.routes.at(rows[2].name).state==MT2FW_MT2_ROUTE_PREPARED&&fake.count("enable:")==0);
    }
    {AllBatchFake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.all_api()));
        for(size_t i=0;i<129;++i)fake.prepare(backend,rows[i]);fake.fail_batch=1;fake.calls.clear();
        CHECK(backend.enable(MH_ALL_HOOKS)==MH_UNKNOWN);CHECK(fake.cohorts.size()==1&&fake.cohorts[0].size()==128);
        CHECK(fake.routes.at(rows[128].name).state==MT2FW_MT2_ROUTE_PREPARED);
        for(size_t i=0;i<128;++i){MT2FW_MT2_RouteReceipt receipt{};CHECK(backend.last_batch_receipt(address(rows[i].rva),receipt));
            CHECK(receipt.state==MT2FW_MT2_ROUTE_UNKNOWN&&receipt.current[0]==0xe9&&std::string(receipt.cause)=="all cohort failed after activation");}
        CHECK(backend.enable(MH_ALL_HOOKS)==MH_UNKNOWN&&fake.cohorts.size()==1&&fake.count("enable:")==0);
    }
    {AllBatchFake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.all_api()));
        CHECK(backend.enable(MH_ALL_HOOKS)==MH_OK&&fake.calls.empty());fake.prepare(backend,rows[0]);fake.calls.clear();
        MH_STATUS result=MH_OK;std::thread worker([&]{result=backend.enable(MH_ALL_HOOKS);});worker.join();
        CHECK(result==MH_ERROR_NOT_INITIALIZED&&fake.calls.empty());}
}
