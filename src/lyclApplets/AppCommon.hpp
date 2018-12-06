#ifndef AppCommon_INCLUDE_ONCE
#define AppCommon_INCLUDE_ONCE

#include <lyclCore/CLUtils.hpp>
#include <lyclCore/Global.hpp>

namespace lycl
{
    //-----------------------------------------------------------------------------
    struct KernelData
    {
        uint32_t uH0;
        uint32_t uH1;
        uint32_t uH2;
        uint32_t uH3;
        uint32_t uH4;
        uint32_t uH5;
        uint32_t uH6;
        uint32_t uH7;

        uint32_t in16;
        uint32_t in17;
        uint32_t in18;

        uint32_t htArg;
    };
    //-----------------------------------------------------------------------------
    struct uint32x8 { uint32_t h[8]; };


    //-----------------------------------------------------------------------------
    // Algorithm utils.
    //-----------------------------------------------------------------------------
    inline bool strIEqual(const std::string& s0, const std::string& s1)
    {
        size_t sz = s0.size();
        if (s1.size() != sz)
        {
            return false;
        }

        for (size_t i = 0; i < sz; ++i)
        {
            if (std::tolower(s0[i]) != std::tolower(s1[i]))
            {
                return false;
            }
        }

        return true;
    }
    //-----------------------------------------------------------------------------
    inline EAlgorithm getAlgorithmFromName(const std::string& algo_name)
    {
        EAlgorithm result;
        if (strIEqual(algo_name, "Lyra2REv3"))
            result = A_Lyra2REv3; 
        else if (strIEqual(algo_name, "Lyra2REv2"))
            result = A_Lyra2REv2; 
        else
            result = A_None;

        return result;
    }
    //-----------------------------------------------------------------------------
    inline void getNameFromAlgorithm(EAlgorithm algo, std::string& out_algo_name)
    {
        switch (algo)
        {
        default:
            {
                out_algo_name = "";
            }break;
        case A_Lyra2REv2:
            {
                out_algo_name = "Lyra2REv2";
            }break;
        case A_Lyra2REv3:
            {
                out_algo_name = "Lyra2REv3";
            }break;
        }
    }

}

#endif // !AppCommon_INCLUDE_ONCE

