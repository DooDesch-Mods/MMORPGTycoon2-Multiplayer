void preflight_reuse_cases(){
    action_session::Context c;action_session::Binding b;std::string e;bool attempted=false,unknown=false;
    auto project=[](){State s;research::Row row;row.key="bridges";row.value.project_present=true;row.value.purchased=true;row.value.work=10;row.value.desired_workers=1;s.research=row;return s;};
    {reset();physical_state=project();State local;std::vector<NativeCursor> cursors;
        need(preflight_local(c,b,physical_state,local,cursors,e)&&state_reads==1&&local==physical_state&&preflight_observed==physical_state&&cursors.size()==1&&cursors[0].sequence==native_cursor.sequence,"private preflight lost actual local State/cursor");
        need(!research_calls,"preflight performed Research mutation");pass("private preflight returns its exact fresh State and NativeCursor without effects");}
    {reset();physical_state=project();need(preflight(c,b,physical_state,e)&&state_reads==1&&!research_calls,"public preflight changed contract");
        pass("public preflight keeps its independent read and original API");}
    {reset();physical_state=project();need(apply(c,b,physical_state,attempted,unknown,e)&&state_reads==1&&!attempted&&!unknown&&!research_calls,"equal apply reread or mutated");
        pass("equal apply consumes one immediate preflight read and no effects");}
    {reset();physical_state=project();desired_state=physical_state;desired_state.research->value.work=11;
        need(preflight(c,b,desired_state,e)&&state_reads==1,"Bridge early preflight fixture failed");
        physical_state.research->value.work=10.5f;
        need(apply(c,b,desired_state,attempted,unknown,e)&&attempted&&!unknown&&state_reads==3&&research_calls==1&&physical_state==desired_state,"Progress apply did not retain before/final reads");
        need(preflight_observed.research->value.work==10.5f,"apply reused stale external preflight across intervening effects");
        pass("Progress path rereads after external effects and retains the complete final read");}
    for(int guard=0;guard<6;++guard){reset();physical_state=project();desired_state=physical_state;desired_state.research->value.work=11;e.clear();c.role=action_session::Role::Client;
        if(guard==0)code_ok=false;
        if(guard==1)c.role=action_session::Role::Host;
        if(guard==2)fail_read=1;
        if(guard==3)research_ok=false;
        if(guard==4)prepare_ok=false;
        if(guard==5)display_ok=false;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&!research_calls,"failed preflight reached an effect");
        const char* causes[]={"original message code failed","tutorial apply requires Client","original native state failed","original research preflight failed","original lifecycle prepare failed","original display admission failed"};
        need(e==causes[guard],"original preflight cause changed");
    }c.role=action_session::Role::Client;pass("all six actual preflight stages retain failures before any effect");
    for(unsigned failed_read:{2u}){reset();physical_state=project();desired_state=physical_state;desired_state.research->value.work=11;fail_read=failed_read;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&attempted&&unknown&&state_reads==failed_read&&research_calls==1&&e=="tutorial native Apply failed: original native state failed","post-effect read failure lost original cause/Unknown");
        const auto cause=e;need(!apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&research_calls==1&&e==cause,"faulted apply replayed Research");
    }pass("Progress final read failure remains Unknown and cannot replay");
    {reset();physical_state=project();desired_state=physical_state;desired_state.research->value.work=11;research_effect_ok=false;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&attempted&&unknown&&state_reads==1&&research_calls==1&&e=="tutorial native Apply failed: original research effect failed","partial Research effect failure changed");
        pass("original partial Research failure retains attempted/Unknown and original cause");}
}
