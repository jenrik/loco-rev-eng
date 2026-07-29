#include "sdl3_intro_video.h"

#include <cassert>
#include <iostream>

int main()
{
#ifndef _WIN32
    using loco::intro::kOriginalLaunchVideoPaths;
    static_assert(kOriginalLaunchVideoPaths.size() == 3);
    assert(kOriginalLaunchVideoPaths[0] == "art-res/video/legospin.avi");
    assert(kOriginalLaunchVideoPaths[1] == "art-res/video/IgSpin.avi");
    assert(kOriginalLaunchVideoPaths[2] == "Video/locoIntr.avi");
#endif
    std::cout << "PASS: original legoSpin -> IgSpin -> locointr launch order\n";
    return 0;
}
