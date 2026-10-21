unsigned duplicate_selection_rng_cases(){
    const std::array<uint64_t,4> expected{7583553001264011721ull,1583152227941235752ull,12324878534382062751ull,14040137720857980887ull};
    const std::array<uint64_t,4> advanced{11299017052131375652ull,4734968277004451891ull,17592428799492746173ull,5508821401572663934ull};
    auto next=[](std::array<uint64_t,4>& s){const auto t=s[1]<<17;s[2]^=s[0];s[3]^=s[1];s[1]^=s[2];s[0]^=s[3];s[2]^=t;s[3]=(s[3]<<45)|(s[3]>>19);};
    unsigned checks=0;
    for(unsigned mode=0;mode<9;++mode){
        auto state=expected;unsigned calls=0,writes=0,guards=0,reads=0;std::string error;
        auto guard=[&](std::string& why){++guards;if(mode==2||(mode==3&&guards==2)){why="fixture current World/owner changed";return false;}return true;};
        auto read=[&](std::array<uint64_t,4>& out,std::string& why){++reads;if(mode==4||(mode==5&&reads==2)){why="fixture RNG read failed";return false;}out=state;return true;};
        auto invoke=[&](std::string& why){++calls;for(unsigned i=0;i<(mode==8?0u:300u);++i)next(state);
            need(state==(mode==8?expected:advanced),"retained native 300-Next transition differs");
            if(mode==1||mode==7){why="fixture original selection SEH";return false;}return true;};
        auto write=[&](const std::array<uint64_t,4>& saved,std::string& why){++writes;
            if(mode==6||mode==7){why="fixture RNG restore failed";return false;}state=saved;return true;};
        const bool ok=dn::restore_selection_random(guard,read,invoke,write,error);
        need(ok==(mode==0||mode==8),"selection RNG boundary accepted failure");
        need(calls==(mode==2||mode==4?0u:1u),"selection original missing or replayed");
        need(writes==(mode==2||mode==3||mode==4?0u:1u),"foreign owner written or RNG restore retried");
        if(mode==0||mode==1||mode==5||mode==8)need(state==expected,"selection presentation RNG escaped");
        if(mode==1||mode==7)need(error.starts_with("fixture original selection SEH"),"original selection failure lost");
        if(mode==7)need(error.find("fixture RNG restore failed")!=std::string::npos,"cleanup failure lost");
        checks+=5;
    }
    // A write that reports success must still provide the exact four-word state.
    auto state=expected;std::string error;unsigned calls=0;
    auto guard=[](std::string&){return true;};
    auto read=[&](std::array<uint64_t,4>& out,std::string&){out=state;return true;};
    auto invoke=[&](std::string&){++calls;next(state);return true;};
    auto write=[](const std::array<uint64_t,4>&,std::string&){return true;};
    need(!dn::restore_selection_random(guard,read,invoke,write,error)&&calls==1&&error.find("readback differs")!=std::string::npos,"unchecked RNG restore accepted");
    return checks+1;
}
