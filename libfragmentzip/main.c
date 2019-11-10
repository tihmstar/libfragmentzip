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
    fragmentzip_t *tt = fragmentzip_open("http://updates-http.cdn-apple.com/2019FallFCS/fullrestores/061-08653/B9079E72-C875-11E9-824B-8D27C2D77BF5/iPhone12,3,iPhone12,5_13.0_17A577_Restore.ipsw");
    if (!tt) {
        printf("failed to open zip\n");
        return 1;
    }
    int rt = fragmentzip_download_file(tt, "Firmware/all_flash/sep-firmware.d431.RELEASE.im4p", "/tmp/sep-firmware.d431.RELEASE.im4pp", NULL);
    
    fragmentzip_close(tt);
    printf("done=%d\n",rt);
    
    return 0;
}
