#pragma once
#include "management_service_graph.h"
#include "management_service_graph_bounds.h"

namespace mt2mp::game::management_service_graph {
struct PointerVector { uintptr_t begin=0,end=0,capacity=0;bool operator==(const PointerVector&)const=default; };
struct ArrayDescriptor { uintptr_t data=0;int32_t count=0,capacity=0;bool operator==(const ArrayDescriptor&)const=default; };
struct BitsDescriptor { uintptr_t begin=0,end=0,capacity=0;uint32_t first=0,last=0;bool operator==(const BitsDescriptor&)const=default; };
template<class Read>bool pointer_vector(Read& at,uintptr_t base,size_t offset,PointerVector& d,std::string& e){
    return at(base,offset,d.begin,e)&&at(base,offset+8,d.end,e)&&at(base,offset+16,d.capacity,e);
}
template<class Read>bool array_descriptor(Read& at,uintptr_t base,size_t offset,ArrayDescriptor& d,std::string& e){
    return at(base,offset+8,d.data,e)&&at(base,offset+16,d.count,e)&&at(base,offset+20,d.capacity,e);
}
template<class Read>bool bits_descriptor(Read& at,uintptr_t internal,BitsDescriptor& d,std::string& e){
    return at(internal,0x18,d.begin,e)&&at(internal,0x20,d.first,e)&&at(internal,0x28,d.end,e)&&
        at(internal,0x30,d.last,e)&&at(internal,0x38,d.capacity,e);
}
// Called only while holding the IslandManager read lock. The caller owns the
// lock, verifies its world and manager, and must release it before SetPrice.
template<class Read>bool read_shape(Read& at,uintptr_t image,uintptr_t shape,Shape& out,size_t& units,std::string& e){
    constexpr size_t limit=1000000;out={};if(units>limit)return fail(e,"Service graph prior geometry exceeds bound");uintptr_t ap=0,internal=0,section_ap=0;uint8_t bounds_valid=0;
    if(!at(shape,0,ap,e)||!at(shape,0x18,internal,e)||!at(shape,0x20,bounds_valid,e)||
       !at(shape,0x24,out.bounds,e)||!at(shape,0x40,section_ap,e))return false;
    if(ap!=image+0x16e0140||!internal||bounds_valid>1||section_ap!=image+0x16eb080)return fail(e,"Service graph shape class/internal descriptor differs");
    for(auto word:out.bounds)if(!finite(word))return fail(e,"Service graph shape bounds are non-finite");
    out.internal_present=true;out.bounds_valid=bounds_valid!=0;
    PointerVector rings;if(!pointer_vector(at,internal,0,rings,e))return false;
    size_t count=0;if(!vector_span(rings.begin,rings.end,rings.capacity,24,limit,count)||count>limit-units)return fail(e,"Service graph ring descriptor invalid");
    units+=count;BitsDescriptor bits;if(!bits_descriptor(at,internal,bits,e))return false;
    if(!bit_span(bits.begin,bits.first,bits.end,bits.last,bits.capacity,count))return fail(e,"Service graph hole-bit descriptor invalid");
    for(size_t i=0;i<count;++i){PointerVector points;if(!pointer_vector(at,rings.begin,i*24,points,e))return false;
        size_t n=0;if(!vector_span(points.begin,points.end,points.capacity,16,limit,n)||n>limit-units)return fail(e,"Service graph ring points descriptor invalid");
        units+=n;Ring ring;uint32_t word=0;if(!at(bits.begin,(i/32)*4,word,e))return false;ring.hole=((word>>(i%32))&1)!=0;
        for(size_t j=0;j<n;++j){std::array<int64_t,2> point{};if(!at(points.begin,j*16,point,e))return false;ring.points.push_back(point);}
        PointerVector after;if(!pointer_vector(at,rings.begin,i*24,after,e))return false;if(after!=points)return fail(e,"Service graph ring points descriptor changed");out.rings.push_back(std::move(ring));
    }
    ArrayDescriptor sections;if(!array_descriptor(at,shape,0x40,sections,e))return false;
    if(!array_span(sections.data,sections.count,sections.capacity,8,limit)||size_t(sections.count)>limit-units)return fail(e,"Service graph section descriptor invalid");
    units+=size_t(sections.count);
    for(int32_t i=0;i<sections.count;++i){uintptr_t section=0,vertex_ap=0;if(!at(sections.data,size_t(i)*8,section,e))return false;if(!section)return fail(e,"Service graph null CrossSection");if(!at(section,0,ap,e)||!at(section,0x40,vertex_ap,e))return false;
        if(ap!=image+0x16fef20||vertex_ap!=image+0x1746160)return fail(e,"Service graph CrossSection or vertex array class differs");
        ArrayDescriptor vertices;if(!array_descriptor(at,section,0x40,vertices,e))return false;
        if(!array_span(vertices.data,vertices.count,vertices.capacity,12,limit)||size_t(vertices.count)>limit-units)return fail(e,"Service graph vertex descriptor invalid");
        units+=size_t(vertices.count);std::vector<std::array<uint32_t,3>> row;
        for(int32_t j=0;j<vertices.count;++j){std::array<uint32_t,3> vertex{};if(!at(vertices.data,size_t(j)*12,vertex,e))return false;for(auto word:vertex)if(!finite(word))return fail(e,"Service graph vertex is non-finite");row.push_back(vertex);}
        ArrayDescriptor after;uintptr_t final_section=0;if(!array_descriptor(at,section,0x40,after,e)||!at(sections.data,size_t(i)*8,final_section,e))return false;if(after!=vertices||section!=final_section)return fail(e,"Service graph vertex descriptor or owner changed");out.sections.push_back(std::move(row));
    }
    PointerVector rings_after;BitsDescriptor bits_after;ArrayDescriptor sections_after;uintptr_t final_internal=0,final_ap=0,final_section_ap=0;uint8_t final_valid=0;std::array<uint32_t,6> final_bounds{};
    if(!pointer_vector(at,internal,0,rings_after,e)||!bits_descriptor(at,internal,bits_after,e)||!array_descriptor(at,shape,0x40,sections_after,e)||
       !at(shape,0x18,final_internal,e)||!at(shape,0,final_ap,e)||!at(shape,0x40,final_section_ap,e)||!at(shape,0x20,final_valid,e)||!at(shape,0x24,final_bounds,e))return false;
    if(rings_after!=rings||bits_after!=bits||sections_after!=sections||final_internal!=internal||final_ap!=image+0x16e0140||final_section_ap!=section_ap||final_valid!=bounds_valid||final_bounds!=out.bounds)
        return fail(e,"Service graph shape descriptor changed during read");
    return true;
}
}
