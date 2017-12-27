//
//  main.c
//  libfragmentzip
//
//  Created by tihmstar on 24.12.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <stdio.h>
#include <libfragmentzip/libfragmentzip.h>

int main(int argc, const char * argv[]) {

    
    fragmentzip_t *tt = fragmentzip_open("http://appldnld.apple.com/ios11.2.1/091-54669-20171213-5FC0AA72-DDFB-11E7-8D39-01E4FB2783B2/iPhone_4.0_64bit_11.2.1_15C153_Restore.ipsw");
    
    int rt = fragmentzip_download_file(tt, "Firmware/all_flash/sep-firmware.n69.RELEASE.im4p", "sep-firmware.n69.RELEASE.im4p", NULL);
    
    fragmentzip_close(tt);
    printf("done=%d\n",rt);
    
    return 0;
}
