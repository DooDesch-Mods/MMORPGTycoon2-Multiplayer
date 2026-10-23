#include "../src/game/party_capture_references.h"
#include <iostream>
#include <vector>

struct Raw {int scope=1;std::vector<int> members;std::vector<int> pools{4};};
struct Fixture {
    Raw before,after;
    int reads=0,pins=0,clears=0,fail_read=0;
    bool owners_valid=true;
    bool incoming_parties=false,incoming_subscribers=false;
    std::string error;
    bool run(){return mt2mp::game::party_frame::capture_reference_pins<Raw>(
        [&](Raw& value,std::string& cause){++reads;if(reads==fail_read){cause="invalid actual owner";return false;}value=reads==1?before:after;return true;},
        [&](std::string& cause){++pins;if(!owners_valid)cause="full reference proof failed";return owners_valid;},
        [&]{++clears;},error,incoming_parties,incoming_subscribers);}
};
int main(){
    int checks=0;
    const auto require=[&](bool okay,const char* name){++checks;if(!okay){std::cerr<<name<<'\n';return false;}return true;};
    Fixture empty;
    if(!require(empty.run()&&empty.reads==2&&empty.pins==0&&empty.clears==1,"empty roster retains two reads and avoids unused owner proof"))return 1;
    Fixture first_invalid;first_invalid.fail_read=1;
    if(!require(!first_invalid.run()&&first_invalid.error=="invalid actual owner"&&first_invalid.pins==0&&first_invalid.clears==0,"failed first read is not empty success"))return 1;
    Fixture second_invalid;second_invalid.fail_read=2;
    if(!require(!second_invalid.run()&&second_invalid.error=="invalid actual owner"&&second_invalid.clears==0,"failed second read preserves cause and owners"))return 1;
    Fixture born;born.after.members={9};
    if(!require(!born.run()&&born.clears==0&&born.pins==0,"empty to nonempty rejects before clearing references"))return 1;
    Fixture changed_scope;changed_scope.after.scope=2;
    if(!require(!changed_scope.run()&&changed_scope.clears==0,"changed actual world owner rejects"))return 1;
    Fixture changed_pool;changed_pool.after.pools={5};
    if(!require(!changed_pool.run()&&changed_pool.clears==0,"changed pool rejects even when members remain empty"))return 1;
    Fixture occupied;occupied.before.members={9};
    if(!require(occupied.run()&&occupied.reads==1&&occupied.pins==1&&occupied.clears==0,"nonempty roster retains full owner proof"))return 1;
    Fixture rejected;rejected.before.members={9};rejected.owners_valid=false;
    if(!require(!rejected.run()&&rejected.pins==1&&rejected.error=="full reference proof failed"&&rejected.clears==0,"invalid nonempty owners stay rejected"))return 1;
    Fixture incoming_party;incoming_party.incoming_parties=true;
    if(!require(incoming_party.run()&&incoming_party.pins==1&&incoming_party.reads==0&&incoming_party.clears==0,"incoming Party retains original proof even with empty local roster"))return 1;
    Fixture incoming_graph;incoming_graph.incoming_subscribers=true;incoming_graph.owners_valid=false;
    if(!require(!incoming_graph.run()&&incoming_graph.pins==1&&incoming_graph.error=="full reference proof failed"&&incoming_graph.clears==0,"incoming Subscriber graph retains original proof failure"))return 1;
    Fixture both;both.incoming_parties=both.incoming_subscribers=true;
    if(!require(both.run()&&both.pins==1&&both.reads==0,"incoming Party and Subscriber use exactly one full proof"))return 1;
    Fixture boundary;
    if(!require(boundary.run()&&boundary.reads==2,"empty pre-native boundary checks live membership"))return 1;
    boundary.reads=0;boundary.before.members={8};boundary.owners_valid=false;
    if(!require(!boundary.run()&&boundary.reads==1&&boundary.pins==1&&boundary.error=="full reference proof failed","new membership after native boundary cannot reuse empty result"))return 1;
    Fixture post_native;post_native.before.scope=post_native.after.scope=2;
    if(!require(post_native.run()&&post_native.reads==2,"next boundary independently samples new valid scope"))return 1;
    std::cout<<checks<<" Party capture reference checks passed\n";
}
