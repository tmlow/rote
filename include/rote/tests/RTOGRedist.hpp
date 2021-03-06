#pragma once
#ifndef ROTE_TESTS_RTOGREDIST_HPP
#define ROTE_TESTS_RTOGREDIST_HPP

using namespace rote;

std::vector<RedistTest >
CreateRTOGTests(const TensorDistribution& distA){
    Unsigned i, j;
    std::vector<RedistTest > ret;
    const Unsigned order = distA.size() - 1;
    ModeArray tensorModes = OrderedModes(order);

    std::vector<ModeArray> redistModesGroups;
    for(i = 1; i <= order; i++){
    	std::vector<ModeArray> newRedistModesGroups = AllCombinations(tensorModes, i);
    	redistModesGroups.insert(redistModesGroups.end(), newRedistModesGroups.begin(), newRedistModesGroups.end());
    }

    for(i = 0; i < redistModesGroups.size(); i++){
    	ModeArray redistModesGroup = redistModesGroups[i];
    	TensorDistribution resDist = distA;

    	SortVector(redistModesGroup);
    	for(j = redistModesGroup.size() - 1; j < redistModesGroup.size(); j--){
    		Mode redistTenMode = redistModesGroup[j];
    		ModeDistribution redistModeDist = resDist[redistTenMode];
    		resDist[resDist.size() - 1].insert(resDist[resDist.size() - 1].end(), redistModeDist.begin(), redistModeDist.end());
    		resDist.erase(resDist.begin() + redistTenMode);
    	}

    	RedistTest test;
    	test.first = resDist;
    	test.second = redistModesGroup;
    	ret.push_back(test);
    }
    return ret;
}

#endif // ifndef ROTE_TESTS_RTOGREDIST_HPP
