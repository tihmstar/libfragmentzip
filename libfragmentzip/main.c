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
    fragmentzip_t *tt = fragmentzip_open("file:///tmp/kk/iPhone_4.7_P3_15.3_19D50_Restore.ipsw");
    if (!tt) {
        printf("failed to open zip\n");
        return 1;
    }
    int rt = fragmentzip_download_file(tt, "BuildManifest.plist", "/tmp/BuildManifest.plist", NULL);
    
    fragmentzip_close(tt);
    printf("done=%d\n",rt);
    
    return 0;
}
