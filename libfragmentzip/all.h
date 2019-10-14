//
//  all.h
//  libfragmentzip
//
//  Created by tihmstar on 02.10.18.
//  Copyright © 2018 tihmstar. All rights reserved.
//

#ifndef all_h
#define all_h

#ifdef DEBUG //this is for developing with Xcode
#define LIBFRAGMENTZIP_VERSION_COUNT "Debug"
#define LIBFRAGMENTZIP_VERSION_SHA "Build: " __DATE__ " " __TIME__
#else
#include <config.h>
#endif

#endif /* all_h */
