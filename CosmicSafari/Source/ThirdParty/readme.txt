Note that one tiny change was required in openvdb in order to build and link against the UE4 openexr - In Types.h the include
  #include <OpenEXR/half.h>
was changed to 
  #include <half.h>
  
Additionally, the UE4 IntelTBB module does not ship with tbb.lib so I manually copied by own binary of tbb.lib to C:\Program Files\Epic Games\4.11\Engine\Source\ThirdParty\IntelTBB\IntelTBB-4.0\lib\Win64\vc14