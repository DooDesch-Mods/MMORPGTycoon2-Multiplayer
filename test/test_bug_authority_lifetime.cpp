#include "bug_authority/lifetime.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::bug_authority;
int checks=0;
void require(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
int main(){
    std::string error;Lifetimes lives;
    Row first{ParentKind::Building,"parent",{BirthKind::Action,"action",0,1},{}};
    Row next=first;next.birth.occurrence=2;
    require(lives.born({first,100,200,300,false},error),"first actual birth");
    require(!lives.born({next,100,200,300,false},error),"live pointer cannot acquire new occurrence");
    require(lives.remove(100,error),"original Tracker removal retires lifetime");
    require(!lives.pointer(100),"retired pointer not live");
    require(lives.born({next,100,200,300,false},error),"allocator reuse after original removal distinct");
    require(!lives.identity(first),"old identity cannot follow reused address");
    require(lives.identity(next)&&lives.identity(next)->bug==100,"new identity resolves same numeric address");
    require(!lives.born({first,101,200,300,false},error),"retired identity cannot replay birth");
    require(!lives.bind_parent(100,ParentKind::Building,"other",200,error),"same receiver cannot change parent key");
    require(!lives.bind_parent(100,ParentKind::Building,"parent",201,error),"same key cannot replace parent receiver");
    require(lives.bind_parent(100,ParentKind::Building,"parent",200,error),"same exact parent retained");
    Row pending=next;pending.birth.occurrence=3;pending.parent_key.clear();
    require(lives.born({pending,101,0,301,false},error),"original constructor can precede parent binding");
    require(lives.bind_parent(101,ParentKind::Npc,"npc",202,error),"original parent bound after action commit");
    Children original{300,400,500,2,{}};
    require(!original.child(401,500,100,error),"foreign writer rejected");
    require(!original.child(400,501,100,error),"foreign ObjectContext rejected");
    require(original.child(400,500,100,error),"first original serialized child");
    require(!original.child(400,500,100,error),"duplicate original child rejected");
    require(!original.finish({100},error),"incomplete original child set rejected");
    require(original.child(400,500,101,error),"second original serialized child");
    require(!original.child(400,500,102,error),"extra child rejected");
    require(!original.finish({101,100},error),"later current ordinal cannot replace original order");
    require(original.finish({100,101},error),"original child order matches completed array");
    Children empty{300,400,500,0,{}};require(empty.finish({},error),"actual original empty set accepted");
    std::cout<<checks<<" actual lifetime/serde checks PASS\n";
}
