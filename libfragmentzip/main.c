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

    
    fragmentzip_t *tt = fragmentzip_open("http://updates-http.cdn-apple.com/2018/ios/091-74856-20180709-813FF9AE-7C1C-11E8-8A6E-A95B544C24EB/iPhone_4.0_64bit_11.4.1_15G77_Restore.ipsw");
    if (!tt) {
        printf("failed to open zip\n");
        return 1;
    }
    int rt = fragmentzip_download_file(tt, "Firmware/all_flash/sep-firmware.n53.RELEASE.im4p", "/tmp/sep-firmware.n53.RELEASE.im4p", NULL);
    
    fragmentzip_close(tt);
    printf("done=%d\n",rt);
    
    return 0;
}
