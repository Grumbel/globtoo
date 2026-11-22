// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "planetary_features.hpp"
#include <cassert>
#include <cmath>
#include <set>
#include <string>

int main() {
    assert(planetary_features.size()==9);
    assert(!find_planetary_feature("Earth"));
    assert(find_planetary_feature("lOkI pAtErA")->body==BodyId::Io);
    assert(find_planetary_feature("tycho")->body==BodyId::Moon);
    assert(find_planetary_feature("OLYMPUS MONS")->body==BodyId::Mars);
    std::set<std::string> names;
    unsigned moon=0,mars=0,io=0;
    for (const auto& feature:planetary_features) {
        assert(feature.body!=BodyId::Earth);
        assert(feature.gazetteer_id>0);
        assert(feature.source_latitude>=-90 && feature.source_latitude<=90);
        assert(feature.source_longitude>=-180 && feature.source_longitude<=360);
        assert(feature_east_longitude(feature)>=-180);
        assert(feature_east_longitude(feature)<180);
        assert(names.emplace(std::string(feature.name)).second);
        assert(find_planetary_feature(feature.name)==&feature);
        if(feature.body==BodyId::Moon) { moon++; assert(feature.latitude_type==FeatureLatitude::Planetographic); }
        if(feature.body==BodyId::Mars) { mars++; assert(feature.latitude_type==FeatureLatitude::Planetocentric); }
        if(feature.body==BodyId::Io) { io++; assert(feature.longitude_direction==FeatureLongitude::WestPositive); }
    }
    assert(moon==3 && mars==3 && io==3);
    const auto& loki=*find_planetary_feature("Loki Patera");
    const auto& pele=*find_planetary_feature("Pele");
    const auto& tvashtar=*find_planetary_feature("Tvashtar Paterae");
    assert(std::abs(feature_east_longitude(loki)-51.20)<.00001);
    assert(std::abs(feature_east_longitude(pele)-104.72)<.00001);
    assert(std::abs(feature_east_longitude(tvashtar)-(-123.53))<.00001);
    assert(std::abs(feature_east_longitude(*find_planetary_feature("Olympus Mons"))-(-134.))<.00001);
    assert(std::abs(feature_east_longitude(*find_planetary_feature("Tycho"))-(-11.22))<.00001);
}
