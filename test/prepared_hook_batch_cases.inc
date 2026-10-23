void prepare_pair(Fake& fake,Backend& backend){
    for(const auto rva:{first,second}){void* original=nullptr;
        CHECK(backend.create(address(rva),detour(),&original)==MH_OK);fake.batch_originals.push_back(original);}
}
void prepared_batch_cases(){
    const std::vector<uintptr_t> targets{image+first,image+second};
    for(unsigned mode=0;mode<=8;++mode){Fake fake;Backend backend;CHECK(fake.bind(backend));fake.query_mode=mode;
        const auto okay=backend.discover_batch(Fake::query,reinterpret_cast<const MT2FW_Host*>(&fake));
        CHECK(okay==(mode<=1));CHECK(backend.batch_available()==(mode==1));CHECK(fake.count("batch")==0);
        if(mode>1)CHECK(!backend.error().empty());
    }
    {Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.discover_batch(Fake::query,reinterpret_cast<const MT2FW_Host*>(&fake)));
        prepare_pair(fake,backend);CHECK(backend.enable_prepared_batch(targets)==MH_OK);
        CHECK(fake.count("enable:")==2&&fake.count("batch")==0);
    }
    {Fake fake;Backend backend;CHECK(fake.bind(backend));prepare_pair(fake,backend);fake.fail_enable=true;
        CHECK(backend.enable_prepared_batch(targets)!=MH_OK);CHECK(fake.count("enable:")==1&&fake.count("batch")==0);
        CHECK(fake.routes.at(second_name).state==MT2FW_MT2_ROUTE_PREPARED);
    }
    for(unsigned mode=0;mode<=4;++mode){Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.api2()));
        prepare_pair(fake,backend);fake.batch_mode=mode;
        const auto result=backend.enable_prepared_batch(targets);CHECK((result==MH_OK)==(mode==0));
        CHECK(fake.count("batch")==1&&fake.count("enable:")==0);
        for(auto target:targets){MT2FW_MT2_RouteReceipt receipt{};CHECK(backend.last_batch_receipt(reinterpret_cast<void*>(target),receipt));
            CHECK(receipt.size==sizeof receipt);
            if(mode==0)CHECK(backend.read_route(reinterpret_cast<void*>(target),receipt));
            else CHECK(!backend.read_route(reinterpret_cast<void*>(target),receipt));
        }
        if(mode==1){MT2FW_MT2_RouteReceipt receipt{};CHECK(backend.last_batch_receipt(address(second),receipt));
            CHECK(receipt.state==MT2FW_MT2_ROUTE_UNKNOWN&&receipt.current[0]==0xea);
            CHECK(std::string(receipt.cause)=="backend partial activation failure");}
        if(mode==4)CHECK(backend.error().find("second target admission failure")!=std::string::npos);
        CHECK(backend.enable_prepared_batch(targets)!=MH_OK);CHECK(fake.count("batch")==1&&fake.count("enable:")==0);
        CHECK(fake.count("remove:")==0);
    }
    for(unsigned mode=0;mode<4;++mode){Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.api2()));prepare_pair(fake,backend);
        auto invalid=targets;if(mode==0)invalid.clear();if(mode==1)invalid[1]=invalid[0];
        if(mode==2)invalid[1]=image+shared;if(mode==3)invalid.resize(129,image+first);
        CHECK(backend.enable_prepared_batch(invalid)!=MH_OK);CHECK(fake.count("batch")==0&&fake.count("enable:")==0);
    }
    {Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.bind_batch(fake.api2()));prepare_pair(fake,backend);fake.fail_read=true;
        CHECK(backend.enable_prepared_batch(targets)!=MH_OK);CHECK(fake.count("batch")==0);}
}
