//
//  libfragmentzip.c
//  libfragmentzip
//
//  Created by tihmstar on 24.12.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include "libfragmentzip.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>

#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)

#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];

#define assure(a) do{ if ((a) == 0){err=1; goto error;} }while(0)
#define safeFree(a) do{ if (a){free(a); a=NULL;} }while(0)

#define nextCD(cd) ((fragmentzip_cd *)(cd->filename+cd->len_filename+cd->len_extra_field+cd->len_file_comment))

typedef struct{
    char *buf;
    size_t size_buf;
    size_t size_downloaded;
    fragmentzip_process_callback_t callback;
}t_downloadBuffer;

static size_t downloadFunction(void* data, size_t size, size_t nmemb, t_downloadBuffer* dbuf) {
    size_t dsize = size*nmemb;
    size_t vsize = 0;
    if (dsize <= dbuf->size_buf - dbuf->size_downloaded){
        vsize = dsize;
    }else{
        vsize = dbuf->size_buf - dbuf->size_downloaded;
    }
    
    memcpy(dbuf->buf+dbuf->size_downloaded, data, vsize);
    dbuf->size_downloaded += vsize;
    
    if (dbuf->callback){
        dbuf->callback((uint)(((double)dbuf->size_downloaded/dbuf->size_buf)*100));
    }
    
    return vsize;
}

STATIC_INLINE void fixEndian_local_file(fragentzip_local_file *lfile){
    if (isBigEndian()) {
        makeLE32(lfile->signature);
        makeLE16(lfile->version);
        makeLE16(lfile->flags);
        makeLE16(lfile->compression);
        makeLE16(lfile->modtime);
        makeLE16(lfile->moddate);
        makeLE32(lfile->crc32);
        makeLE32(lfile->size_compressed);
        makeLE32(lfile->size_uncompressed);
        makeLE16(lfile->len_filename);
        makeLE16(lfile->len_extra_field);
    }
}

STATIC_INLINE void fixEndian_data_descriptor(fragmentzip_data_descriptor *ddesc){
    if (isBigEndian()) {
        makeLE32(ddesc->crc32);
        makeLE32(ddesc->size_compressed);
        makeLE32(ddesc->size_uncompressed);
    }
}

STATIC_INLINE void fixEndian_end_of_cd(fragmentzip_end_of_cd *cde){
    if (isBigEndian()) {
        makeLE32(cde->signature);
        makeLE16(cde->disk_cur_number);
        makeLE16(cde->disk_cd_start_number);
        makeLE16(cde->cd_disk_number);
        makeLE16(cde->cd_entries);
        makeLE32(cde->cd_size);
        makeLE32(cde->cd_start_offset);
        makeLE16(cde->comment_len);
    }
}

STATIC_INLINE void fixEndian_cd(fragmentzip_cd *cd, uint entries){
    if (isBigEndian()) {
        for (int i=0; i<entries; i++) {
            makeLE32(cd->signature);
            makeLE16(cd->version);
            makeLE16(cd->pkzip_version_needed);
            makeLE16(cd->flags);
            makeLE16(cd->compression);
            makeLE16(cd->modtime);
            makeLE16(cd->moddate);
            makeLE32(cd->crc32);
            makeLE32(cd->size_compressed);
            makeLE32(cd->size_uncompressed);
            makeLE16(cd->len_filename);
            makeLE16(cd->len_extra_field);
            makeLE16(cd->len_file_comment);
            makeLE16(cd->disk_num);
            makeLE16(cd->internal_attribute);
            makeLE32(cd->external_attribute);
            makeLE32(cd->local_header_offset);
            
            cd = nextCD(cd);
        }
    }
}

CASSERT(sizeof(fragmentzip_cd) == 47, fragmentzip_cd_size_is_wrong);
CASSERT(sizeof(fragmentzip_end_of_cd) == 22, fragmentzip_end_of_cd_size_is_wrong);

fragmentzip_t *fragmentzip_open_extended(const char *url, CURL *mcurl){
    
    int err = 0;
    fragmentzip_t *info = NULL;
    t_downloadBuffer *dbuf = NULL;
    fragmentzip_end_of_cd *cde = NULL;
    
    assure(dbuf = malloc(sizeof(t_downloadBuffer)));
    bzero(dbuf, sizeof(t_downloadBuffer));
    
    assure(info = (fragmentzip_t*)malloc(sizeof(fragmentzip_t)));
    bzero(info,sizeof(fragmentzip_t));
    
    assure(info->url = strdup(url));
    
    assure(info->mcurl = mcurl);
    
    curl_easy_setopt(info->mcurl, CURLOPT_CONNECTTIMEOUT, 30L); //30 sec connect timeout
    curl_easy_setopt(info->mcurl, CURLOPT_URL, info->url);
    curl_easy_setopt(info->mcurl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(info->mcurl, CURLOPT_NOBODY, 1);
    
    
    assure(curl_easy_perform(info->mcurl) == CURLE_OK);
    
    double len = 0;
    curl_easy_getinfo(info->mcurl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);
    assure((info->length = len)>sizeof(fragmentzip_end_of_cd));
    
    //get end of central directory
    assure(dbuf->buf = malloc(dbuf->size_buf = sizeof(fragmentzip_end_of_cd)));
    
    curl_easy_setopt(info->mcurl, CURLOPT_WRITEFUNCTION, &downloadFunction);
    curl_easy_setopt(info->mcurl, CURLOPT_WRITEDATA, dbuf);
    
    char downloadRange[100] = {0};
    snprintf(downloadRange, sizeof(downloadRange), "%llu-%llu",info->length - sizeof(fragmentzip_end_of_cd), info->length-1);
    
    curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
    curl_easy_setopt(info->mcurl, CURLOPT_HTTPGET, 1);

    assure(curl_easy_perform(info->mcurl) == CURLE_OK);
    
    assure(strncmp(dbuf->buf, "\x50\x4b\x05\x06", 4) == 0);
    
    cde = (fragmentzip_end_of_cd*)dbuf->buf;
    fixEndian_end_of_cd(cde);
    
    bzero(downloadRange, sizeof(downloadRange));
    snprintf(downloadRange, sizeof(downloadRange), "%u-%llu",cde->cd_start_offset, info->length-1);
    curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
    
    dbuf->size_downloaded = 0;
    dbuf->size_buf = cde->cd_size + sizeof(fragmentzip_end_of_cd);
    assure(dbuf->buf = malloc(dbuf->size_buf));
    
    assure(curl_easy_perform(info->mcurl) == CURLE_OK);
    
    assure(strncmp(dbuf->buf, "\x50\x4b\x01\x02", 4) == 0);
    
    info->cd = (fragmentzip_cd*)dbuf->buf;
    info->cd_end = (fragmentzip_end_of_cd*)(((char*)info->cd)+cde->cd_size);
    
    fixEndian_cd(info->cd,cde->cd_entries);
    fixEndian_end_of_cd(info->cd_end); //fix the end_of_central_directoy at the end
    
error:
    if (err) {
        fragmentzip_close(info);
        safeFree(dbuf->buf);
        safeFree(dbuf);
    }
    safeFree(cde);
    safeFree(dbuf);
    
    return (err) ? NULL : info;
}

fragmentzip_t *fragmentzip_open(const char *url){
    return fragmentzip_open_extended(url, curl_easy_init());
}

fragmentzip_cd *getCDForPath(fragmentzip_t *info, const char *path){
    fragmentzip_cd *curr = info->cd;
    for (int i=0; i<info->cd_end->cd_entries; i++) {
        
        if (strncmp(curr->filename, path, strlen(path)) == 0) return curr;
        
        curr = nextCD(curr);
    }

    return NULL;
}

int fragmentzip_download_file(fragmentzip_t *info, const char *remotepath, const char *savepath, fragmentzip_process_callback_t callback){
    int err = 0;
    t_downloadBuffer *compressed = NULL;
    fragentzip_local_file *lfile = NULL;
    char *uncompressed = NULL;
    FILE *f = NULL;
    
    
    fragmentzip_cd *rfile = NULL;
    assure(rfile = getCDForPath(info, remotepath));
    
    assure(compressed = (t_downloadBuffer*)malloc(sizeof(t_downloadBuffer)));
    bzero(compressed, sizeof(t_downloadBuffer));
    
    compressed->callback = callback;
    
    assure(compressed->buf = (char*)malloc(compressed->size_buf = sizeof(fragentzip_local_file)-1));
    
    char downloadRange[100] = {0};
    snprintf(downloadRange, sizeof(downloadRange), "%u-%u",rfile->local_header_offset,(unsigned)(rfile->local_header_offset + compressed->size_buf-1));
    
    curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
    curl_easy_setopt(info->mcurl, CURLOPT_WRITEDATA, compressed);
    
    assure(curl_easy_perform(info->mcurl) == CURLE_OK);
    assure(strncmp(compressed->buf, "\x50\x4b\x03\x04", 4) == 0);
    
    lfile = (fragentzip_local_file*)compressed->buf;
    fixEndian_local_file(lfile);
    
    compressed->size_downloaded = 0;
    assure(compressed->buf = malloc(compressed->size_buf = rfile->size_compressed));
    
    bzero(downloadRange,sizeof(downloadRange));
    
    uint start = (uint)rfile->local_header_offset + sizeof(fragentzip_local_file)-1 + lfile->len_filename + lfile->len_extra_field;
    snprintf(downloadRange, sizeof(downloadRange), "%u-%u",start,(uint)(start+compressed->size_buf-1));
    curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
    
    assure(curl_easy_perform(info->mcurl) == CURLE_OK);
    
    
    assure(uncompressed = malloc(rfile->size_uncompressed));
    //file downloaded, now unpack it
    switch (lfile->compression) {
        case 8: //defalted
        {
            z_stream strm = {};
            inflateInit2(&strm, -MAX_WBITS);
            
            strm.avail_in = rfile->size_compressed;
            strm.next_in = (z_const Bytef *)compressed->buf;
            strm.avail_out = rfile->size_uncompressed;
            strm.next_out = (Bytef *)uncompressed;
            
            inflate(&strm, Z_FINISH);
            assure(strm.msg == NULL);
            inflateEnd(&strm);
        }
            break;
            
        default:
            printf("[Error] unknown compression method\n");
            assure(0);
            break;
    }
    
    assure(crc32(0, (unsigned char *)uncompressed, rfile->size_uncompressed) == rfile->crc32);
    
    //file unpacked, now save it
    assure(f = fopen(savepath, "w"));
    assure(fwrite(uncompressed, 1, rfile->size_uncompressed, f) == rfile->size_uncompressed);
    
    
    
error:
    if (compressed){
        safeFree(compressed->buf);
        safeFree(compressed);
    }
    safeFree(uncompressed);
    safeFree(lfile);
    if (f) fclose(f);
    
    return err;
}


void fragmentzip_close(fragmentzip_t *info){
    if (info){
        safeFree(info->url);
        if (info->mcurl) curl_free(info->mcurl);
        safeFree(info->cd); //don't free info->cd_end because it points into the same buffer
        
        free(info);
    }
}
