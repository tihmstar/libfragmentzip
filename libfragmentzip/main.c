//
//  main.c
//  libfragmentzip
//
//  Created by tihmstar on 24.12.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <stdio.h>
#include "libfragmentzip.h"

int main(int argc, const char * argv[]) {

    
    fragmentzip_t *tt = fragmentzip_open("http://appldnld.apple.com/ios10.2seed/031-93643-20161207-6173C962-BBD8-11E6-B93D-977FE47229E1/iPhone_4.0_64bit_10.2_14C92_Restore.ipsw");
    
    int rt = fragmentzip_download_file(tt, "BuildManifest.plist", "/tmp/asdasd.plist", NULL);
    
    fragmentzip_close(tt);
    printf("done=%d\n",rt);
    
    return 0;
}
