#include "character_catalogue_proof.h"
#include <iostream>
#include <stdexcept>
namespace {
namespace proof=mt2mp::game::character_catalogue_proof;
namespace definitions=mt2mp::game::character_definition_identity;
int checks=0;
void expect(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
definitions::Census census(){definitions::Census value;
    definitions::Definition base;base.key="character:1:0";base.kind=1;base.id=0;base.uid=100;base.object=1000;
    definitions::Definition variant=base;variant.key="character:1:1";variant.id=1;variant.uid=101;variant.object=1001;
    variant.parent=1000;variant.parent_uid=100;variant.edited=true;variant.released=true;
    definitions::Definition deleted=base;deleted.key="character:1:2";deleted.id=2;deleted.uid=0;deleted.object=1002;deleted.deleted=true;
    value.definitions={base,variant,deleted};value.arrays[1].count=3;value.arrays[1].members={1000,1001,1002};
    definitions::Collection variants;variants.owner=1000;variants.ids={{1,1}};value.collections.push_back(variants);return value;
}
void run(){using json=nlohmann::json;std::string error;const auto before=census();
    const std::map<std::string,std::string> records{{"character:1:0",std::string(64,'a')},{"character:1:1",std::string(64,'b')}};
    json rows;expect(proof::rows(before,records,rows,error),"complete original base/variant/tombstone catalogue");
    expect(rows.size()==3&&rows[1]["parent"]==0&&rows[0]["variants"]==json::array({1}),"native variant relationship and order");
    expect(rows[2]["record_sha256"]==""&&rows[2]["deleted"]==true,"deleted slot retained without native Record capture");
    auto local=before;local.manager=9000;local.state=8000;
    for(auto& row:local.definitions){row.object+=100;row.uid+=200;if(row.parent){row.parent+=100;row.parent_uid+=200;}}
    local.collections[0].owner+=100;local.arrays[1].members={1100,1101,1102};json other;
    expect(proof::rows(local,records,other,error)&&other==rows,"peer local receivers/full UIDs never become wire identity");
    auto changed=records;changed["character:1:1"]=std::string(64,'c');
    expect(proof::rows(before,changed,other,error)&&other!=rows,"full original definition changes remain semantic");
    changed=records;changed.erase("character:1:1");expect(!proof::rows(before,changed,other,error),"missing live record rejects");
    changed=records;changed["character:1:2"]=std::string(64,'d');expect(!proof::rows(before,changed,other,error),"tombstone cannot claim live record");
    changed=records;changed["character:4:0"]=std::string(64,'d');expect(!proof::rows(before,changed,other,error),"foreign definition record rejects");
    changed=records;changed["character:1:1"]=std::string(63,'a');expect(!proof::rows(before,changed,other,error),"record SHA size exact");
    changed=records;changed["character:1:1"]=std::string(64,'Z');expect(!proof::rows(before,changed,other,error),"record SHA encoding exact");
    local=before;local.definitions[1].parent_uid=999;expect(!proof::rows(local,records,other,error),"parent full UID mismatch rejects");
    local=before;local.definitions[1].parent=777;expect(!proof::rows(local,records,other,error),"unknown current parent rejects");
    local=before;local.definitions[1].id=4;expect(!proof::rows(local,records,other,error),"sparse/invented manager ID rejects");
    local=before;local.definitions[1].key="character:4:1";expect(!proof::rows(local,records,other,error),"foreign canonical key rejects");
    local=before;local.arrays[1].count=4;expect(!proof::rows(local,records,other,error),"complete original count required");
    local=before;local.arrays[1].members.pop_back();expect(!proof::rows(local,records,other,error),"complete original member span required");
    local=before;local.collections[0].ids[0][0]=4;expect(!proof::rows(local,records,other,error),"variant kind cannot cross families");
    local=before;local.definitions[1].released=false;
    expect(proof::rows(local,records,other,error)&&other!=rows,"release flag included independently of original record hash");
    local=before;local.definitions[1].edited=false;
    expect(proof::rows(local,records,other,error)&&other!=rows,"edited flag included independently of original record hash");
    local=before;auto born=local.definitions[2];born.uid=200;born.deleted=false;local.definitions[2]=born;
    changed=records;changed["character:1:2"]=std::string(64,'e');
    expect(proof::rows(local,changed,other,error)&&other!=rows,"native reused tombstone is a new current definition");
}
}
int main(){try{run();std::cout<<checks<<" Character catalogue checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<"Character catalogue: "<<exception.what()<<'\n';return 1;}}
