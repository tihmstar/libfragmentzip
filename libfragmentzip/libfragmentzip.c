//
//  libfragmentzip.c
//  libfragmentzip
//
//  Created by tihmstar on 24.12.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//


#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include <libgeneral/macros.h>

#ifdef HAVE_FSEEKO
#define _LARGEFILE_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>
#include <assert.h>
#include <inttypes.h>
#include <libfragmentzip/libfragmentzip.h>

#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)
#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];


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
        dbuf->callback((unsigned int)(((double)dbuf->size_downloaded/dbuf->size_buf)*100));
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

STATIC_INLINE void fixEndian_end_of_cd64(fragmentzip64_end_of_cd *cde64){
    if (isBigEndian()) {
        makeLE32(cde64->signature);
        makeLE64(cde64->end_of_cd_size);
        makeLE16(cde64->version_made);
        makeLE16(cde64->version_needed);
        makeLE32(cde64->disk_cur_number);
        makeLE32(cde64->disk_cd_start_number);
        makeLE64(cde64->cd_disk_number);
        makeLE64(cde64->cd_entries);
        makeLE64(cde64->cd_size);
        makeLE64(cde64->cd_start_offset);
    }
}

STATIC_INLINE void fixEndian_end_of_cd_locator64(fragmentzip64_end_of_cd_locator *cdle64){
    if (isBigEndian()) {
        makeLE32(cdle64->signature);
        makeLE32(cdle64->disk_cd_start_number);
        makeLE64(cdle64->end_of_cd_record_offset);
        makeLE32(cdle64->cd_disk_number);
    }
}

STATIC_INLINE void fixEndian_extended_information_extra_field64(fragmentzip64_extended_information_extra_field *eief64){
    if (isBigEndian()) {
        makeLE16(eief64->field_tag);
        makeLE16(eief64->field_size);
    }
}

STATIC_INLINE int fixEndian_cd(fragmentzip_t *info){
    int err = 0;
    fragmentzip_cd *cd = info->cd;
    uint64_t entries = info->cd_entries;
    if (isBigEndian()) {
        for (uint64_t i=0; i<entries; i++) {
            cassure((char*)cd-(char*)info->cd <= info->length-sizeof(fragmentzip_cd)); //sanity check

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
            
            cd = fragmentzip_nextCD(cd);
        }
    }

error:
    return err;
}

uint32_t crc32_for_byte(uint32_t r) {
    for(int j = 0; j < 8; ++j)
        r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
    return r ^ (uint32_t)0xFF000000L;
}

uint32_t mycrc32(const void *data, size_t n_bytes) {
    uint32_t crc = 0;
    static uint32_t table[0x100] = {};
    if(!*table)
        for(uint32_t i = 0; i < 0x100; ++i)
            table[i] = crc32_for_byte(i);
    for(size_t i = 0; i < n_bytes; ++i)
        crc = table[(uint8_t)crc ^ ((uint8_t*)data)[i]] ^ crc >> 8;
    return crc;
}

CASSERT(sizeof(fragmentzip_cd) == 46, fragmentzip_cd_size_is_wrong);
CASSERT(sizeof(fragmentzip_end_of_cd) == 22, fragmentzip_end_of_cd_size_is_wrong);

fragmentzip_t *fragmentzip_open_extended(const char *url, CURL *mcurl){
    int err = 0;
    fragmentzip_t *info = NULL;
    t_downloadBuffer *dbuf = NULL;
    
    CURLcode cc = CURLE_OK;
    
    cassure(dbuf = malloc(sizeof(t_downloadBuffer)));
    memset(dbuf, 0, sizeof(t_downloadBuffer));
    
    cassure(info = (fragmentzip_t*)malloc(sizeof(fragmentzip_t)));
    memset(info, 0, sizeof(fragmentzip_t));
    
    cassure(info->url = strdup(url));
    cassure(info->mcurl = mcurl);
    
    if (strncmp(info->url, "file://", sizeof("file://")-1) == 0) {
        cassure(info->localFile = fopen(info->url+strlen("file://"),"rb"));
    }else{
        curl_easy_setopt(info->mcurl, CURLOPT_CONNECTTIMEOUT, 30L); //30 sec connect timeout
        curl_easy_setopt(info->mcurl, CURLOPT_URL, info->url);
        curl_easy_setopt(info->mcurl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(info->mcurl, CURLOPT_NOBODY, 1);
    }
    
    cassure(info->localFile || info->mcurl);
    
    if (!info->localFile) {
        cassure((cc = curl_easy_perform(info->mcurl)) == CURLE_OK);
        double len = 0;
        curl_easy_getinfo(info->mcurl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);
        cassure((info->length = len)>sizeof(fragmentzip_end_of_cd));
    }else{
        cassure(!fseek(info->localFile, 0, SEEK_END));
        cassure((info->length = ftell(info->localFile))>sizeof(fragmentzip_end_of_cd));
    }
    
    //get end of central directory
    cassure(dbuf->buf = malloc(dbuf->size_buf = sizeof(fragmentzip_end_of_cd)));
    
    char downloadRange[100] = {0};
    snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64 ,info->length - sizeof(fragmentzip_end_of_cd), info->length-1);
    
    if (!info->localFile) {
        curl_easy_setopt(info->mcurl, CURLOPT_WRITEFUNCTION, &downloadFunction);
        curl_easy_setopt(info->mcurl, CURLOPT_WRITEDATA, dbuf);
        curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
        curl_easy_setopt(info->mcurl, CURLOPT_HTTPGET, 1);


        curl_easy_setopt(info->mcurl, CURLOPT_CONNECTTIMEOUT, 15L); //30 sec connect timeout
        curl_easy_setopt(info->mcurl, CURLOPT_FOLLOWLOCATION, 1);
        
        cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
    }else{
        uint64_t doffset = atoll(downloadRange);
        cassure(!fseek(info->localFile, doffset, SEEK_SET));
        cassure(dbuf->size_buf == fread(dbuf->buf, 1, dbuf->size_buf, info->localFile));
    }
    
    cassure(strncmp(dbuf->buf, "\x50\x4b\x05\x06", 4) == 0);
    
    info->internal.cd_end = (fragmentzip_end_of_cd*)dbuf->buf;dbuf->buf = NULL;
    fixEndian_end_of_cd(info->internal.cd_end);
    
    if (info->internal.cd_end->disk_cur_number        == (uint16_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->disk_cd_start_number   == (uint16_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->cd_disk_number         == (uint16_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->cd_entries             == (uint16_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->cd_size                == (uint32_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->cd_start_offset        == (uint32_t)-1) info->isZIP64 |= 1;
    if (info->internal.cd_end->comment_len            == (uint16_t)-1) info->isZIP64 |= 1;

    if (info->isZIP64) {
        //get fragmentzip64_end_of_cd_locator
        memset(downloadRange, 0, sizeof(downloadRange));
        snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64,info->length - sizeof(fragmentzip_end_of_cd) - sizeof(fragmentzip64_end_of_cd_locator), info->length - sizeof(fragmentzip_end_of_cd));
        dbuf->size_buf = info->internal.cd_end->cd_size + sizeof(fragmentzip64_end_of_cd_locator);
        
        dbuf->size_downloaded = 0;
        cassure(dbuf->buf = malloc(dbuf->size_buf));
        
        if (!info->localFile) {
            curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
            cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
        }else{
            uint64_t doffset = atoll(downloadRange);
            uint64_t dsize = atoll(strchr(downloadRange, '-')+1) - doffset;
            cassure(!fseek(info->localFile, doffset, SEEK_SET));
            cassure(dsize == fread(dbuf->buf, 1, dsize, info->localFile));
        }
        cassure(strncmp(dbuf->buf, "\x50\x4b\x06\x07", 4) == 0);
        info->internal.cd64_end_locator = (fragmentzip64_end_of_cd_locator*)dbuf->buf;dbuf->buf = NULL;
        fixEndian_end_of_cd_locator64(info->internal.cd64_end_locator);

        //get fragmentzip64_end_of_cd
        memset(downloadRange, 0, sizeof(downloadRange));
        snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64,info->internal.cd64_end_locator->end_of_cd_record_offset, info->internal.cd64_end_locator->end_of_cd_record_offset+sizeof(fragmentzip64_end_of_cd)-1);
        
        dbuf->size_buf = sizeof(fragmentzip64_end_of_cd);
        
        dbuf->size_downloaded = 0;
        cassure(dbuf->buf = malloc(dbuf->size_buf));
        
        if (!info->localFile) {
            curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
            cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
        }else{
            uint64_t doffset = atoll(downloadRange);
            cassure(!fseek(info->localFile, doffset, SEEK_SET));
            cassure(dbuf->size_buf == fread(dbuf->buf, 1, dbuf->size_buf, info->localFile));
        }
        
        cassure(strncmp(dbuf->buf, "\x50\x4b\x06\x06", 4) == 0);
        
        info->internal.cd64_end = (fragmentzip64_end_of_cd*)dbuf->buf; dbuf->buf = NULL;
        fixEndian_end_of_cd64(info->internal.cd64_end);
        
        memset(downloadRange, 0, sizeof(downloadRange));
        dbuf->size_buf = info->internal.cd64_end->cd_size + sizeof(fragmentzip64_end_of_cd);
        snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64,info->internal.cd64_end->cd_start_offset, info->internal.cd64_end->cd_start_offset+dbuf->size_buf-1);

    }else{
        memset(downloadRange, 0, sizeof(downloadRange));
        dbuf->size_buf = info->internal.cd_end->cd_size + sizeof(fragmentzip_end_of_cd);
        snprintf(downloadRange, sizeof(downloadRange), "%" PRIu32 "-%" PRIu64,info->internal.cd_end->cd_start_offset, info->internal.cd_end->cd_start_offset+dbuf->size_buf-1);
    }
    
    dbuf->size_downloaded = 0;
    cassure(dbuf->buf = malloc(dbuf->size_buf));
    
    if (!info->localFile) {
        curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
        cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
    }else{
        uint64_t doffset = atoll(downloadRange);
        cassure(!fseek(info->localFile, doffset, SEEK_SET));
        cassure(dbuf->size_buf == fread(dbuf->buf, 1, dbuf->size_buf, info->localFile));
    }
    
    cassure(strncmp(dbuf->buf, "\x50\x4b\x01\x02", 4) == 0);
    
    info->cd = (fragmentzip_cd*)dbuf->buf; dbuf->buf = NULL;
    fixEndian_cd(info);

    if (info->isZIP64) {
        info->cd_entries = info->internal.cd64_end->cd_entries;
    }else{
        info->cd_entries = info->internal.cd_end->cd_entries;
    }
    
    //sanity check data
    fragmentzip_cd *curr = info->cd;
    for (int i=0; i<info->cd_entries; i++) {
        int64_t checkLen = info->length - ((char*)curr-(char*)info->cd) - sizeof(fragmentzip_cd);
        cassure(checkLen > 0); //sanity check
        cassure(checkLen > curr->len_filename + curr->len_extra_field + curr->len_file_comment); //sanity check
        curr = fragmentzip_nextCD(curr);
    }
    
error:
    if (err) {
        fragmentzip_close(info);
        safeFree(dbuf->buf);
        safeFree(dbuf);
    }
    safeFree(dbuf);
    
    return (err) ? NULL : info;
}

fragmentzip_cd *fragmentzip_getNextCD(fragmentzip_cd *cd){
    return fragmentzip_nextCD(cd);
}

int fragmentzip_getFileInfo(fragmentzip_cd *cd, uint64_t *uncompressedSize, uint64_t *compressedSize, uint64_t *headerOffset, uint32_t *disk_num){
    int err = 0;
    int extraIndex = 0;
    fragmentzip64_extended_information_extra_field *cdinfo = NULL;
    struct{
        uint64_t uncompressedSize;
        uint64_t compressedSize;
        uint64_t headerOffset;
        uint32_t disk_num;
    }fields = {};
    
    if (cd->len_extra_field>=sizeof(fragmentzip64_extended_information_extra_field)) {
        cdinfo = (fragmentzip64_extended_information_extra_field*)(((uint8_t*)(cd+1)) + cd->len_filename);
        fixEndian_extended_information_extra_field64(cdinfo);
    }
    
    if ((fields.uncompressedSize = cd->size_uncompressed) == (uint32_t)-1){
        cassure(cdinfo);
        cassure(cd->len_extra_field>=sizeof(fragmentzip64_extended_information_extra_field) + sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        cassure(cdinfo->field_size >= sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        fields.uncompressedSize = cdinfo->extrafield[extraIndex++];
        makeLE64(fields.uncompressedSize);
    }
    
    if ((fields.compressedSize = cd->size_compressed) == (uint32_t)-1){
        cassure(cdinfo);
        cassure(cd->len_extra_field>=sizeof(fragmentzip64_extended_information_extra_field) + sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        cassure(cdinfo->field_size >= sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        fields.compressedSize = cdinfo->extrafield[extraIndex++];
        makeLE64(fields.compressedSize);
    }
    
    if ((fields.headerOffset = cd->local_header_offset) == (uint32_t)-1){
        cassure(cdinfo);
        cassure(cd->len_extra_field>=sizeof(fragmentzip64_extended_information_extra_field) + sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        cassure(cdinfo->field_size >= sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        fields.headerOffset = cdinfo->extrafield[extraIndex++];
        makeLE64(fields.headerOffset);
    }
    if ((fields.disk_num = cd->disk_num) == (uint16_t)-1){
        cassure(cdinfo);
        cassure(cd->len_extra_field>=sizeof(fragmentzip64_extended_information_extra_field) + sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        cassure(cdinfo->field_size >= sizeof(uint64_t)*extraIndex + sizeof(uint64_t));
        fields.disk_num = (uint32_t)cdinfo->extrafield[extraIndex++];
        makeLE32(fields.disk_num);
    }
    
    if (uncompressedSize)   *uncompressedSize = fields.uncompressedSize;
    if (compressedSize)     *compressedSize = fields.compressedSize;
    if (headerOffset)       *headerOffset = fields.headerOffset;
    if (disk_num)           *disk_num = fields.disk_num;
    
error:
    return err;
}

fragmentzip_t *fragmentzip_open(const char *url){
    return fragmentzip_open_extended(url, curl_easy_init());
}

fragmentzip_cd *fragmentzip_getCDForPath(fragmentzip_t *info, const char *path){
    int err = 0;
    size_t path_len = strlen(path);

    fragmentzip_cd *curr = info->cd;
    for (int i=0; i<info->cd_entries; i++) {
        int64_t checkLen = info->length - ((char*)curr-(char*)info->cd) - sizeof(fragmentzip_cd);
        cassure(checkLen > 0); //sanity check
        cassure(checkLen > curr->len_filename); //sanity check
        
        if (path_len == curr->len_filename && strncmp(curr->filename, path, path_len) == 0) return curr;
        
        curr = fragmentzip_nextCD(curr);
    }
    
error:
    if (err) {
        /* we got an error!
         we may handle it here, or just return NULL, since that's what this function is supposed
         to return in case of an error anyways.
        */
    }
    return NULL;
}

int fragmentzip_download_to_memory(fragmentzip_t *info, const char *remotepath, char **outBuf, size_t *outSize, fragmentzip_process_callback_t callback){
    int err = 0;
    t_downloadBuffer *compressed = NULL;
    fragentzip_local_file *lfile = NULL;
    char *uncompressed = NULL;
    uint64_t uncompressedSize = 0;
    uint64_t compressedSize = 0;
    uint64_t headerOffset = 0;
    z_stream strm = {0};

    *outBuf = NULL;
    *outSize = 0;

    fragmentzip_cd *rfile = NULL;
    cassure(rfile = fragmentzip_getCDForPath(info, remotepath));
    
    cassure(!fragmentzip_getFileInfo(rfile, &uncompressedSize, &compressedSize, &headerOffset, NULL));
    
    cassure(compressed = (t_downloadBuffer*)malloc(sizeof(t_downloadBuffer)));
    memset(compressed, 0, sizeof(t_downloadBuffer));
    
    compressed->callback = callback;
    
    cassure(compressed->buf = (char*)malloc(compressed->size_buf = sizeof(fragentzip_local_file)));
    
    char downloadRange[100] = {0};
    snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64,headerOffset,headerOffset + compressed->size_buf-1);
    
    if (!info->localFile) {
        curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
        curl_easy_setopt(info->mcurl, CURLOPT_WRITEDATA, compressed);
        cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
    }else{
        uint64_t doffset = atoll(downloadRange);
        cassure(!fseek(info->localFile, doffset, SEEK_SET));
        cassure(compressed->size_buf == fread(compressed->buf, 1, compressed->size_buf, info->localFile));
    }
    
    cassure(strncmp(compressed->buf, "\x50\x4b\x03\x04", 4) == 0);
    
    lfile = (fragentzip_local_file*)compressed->buf;
    fixEndian_local_file(lfile);
    
    compressed->size_downloaded = 0;
    cassure(compressed->buf = malloc(compressed->size_buf = compressedSize));
    
    memset(downloadRange, 0, sizeof(downloadRange));
    
    uint64_t start = headerOffset + sizeof(fragentzip_local_file) + lfile->len_filename + lfile->len_extra_field;
    snprintf(downloadRange, sizeof(downloadRange), "%" PRIu64 "-%" PRIu64,start,start+compressed->size_buf-1);
    
    if (!info->localFile) {
        curl_easy_setopt(info->mcurl, CURLOPT_RANGE, downloadRange);
        cassure(curl_easy_perform(info->mcurl) == CURLE_OK);
    }else{
        uint64_t doffset = atoll(downloadRange);
        cassure(!fseek(info->localFile, doffset, SEEK_SET));
        cassure(compressed->size_buf == fread(compressed->buf, 1, compressed->size_buf, info->localFile));
    }
    
    cassure(uncompressed = calloc(1,uncompressedSize));
    //file downloaded, now unpack it
    switch (lfile->compression) {
        case 0: //store
        {
            cassure(compressedSize == uncompressedSize);
            memcpy(uncompressed, compressed->buf, uncompressedSize);
            break;
        }
        case 8: //defalted
        {
            cassure(inflateInit2(&strm, -MAX_WBITS) >= 0);
            
            strm.avail_in = (uInt)compressedSize;
            strm.next_in = (Bytef *)compressed->buf;
            strm.avail_out = (uInt)uncompressedSize;
            strm.next_out = (Bytef *)uncompressed;
            
            cassure(inflate(&strm, Z_FINISH) > 0);
            cassure(strm.msg == NULL);
            inflateEnd(&strm);
        }
            break;
            
        default:
            printf("[Error] unknown compression method\n");
            cassure(0);
            break;
    }
    
    cassure(mycrc32((unsigned char *)uncompressed, uncompressedSize) == rfile->crc32);
    
    *outBuf = uncompressed; uncompressed = NULL;
    *outSize = uncompressedSize; uncompressedSize = 0;
    
error:
    if (compressed){
        safeFree(compressed->buf);
        safeFree(compressed);
    }
    safeFree(uncompressed);
    safeFree(lfile);
    inflateEnd(&strm);
    
    return err;
}

int fragmentzip_download_file(fragmentzip_t *info, const char *remotepath, const char *savepath, fragmentzip_process_callback_t callback){
    int err = 0;
    char *fileBuf = NULL;
    size_t fileBufSize = 0;
    FILE *f = NULL;

    if (!(err = fragmentzip_download_to_memory(info, remotepath, &fileBuf, &fileBufSize, callback))){
        //file unpacked, now save it
        cassure(f = fopen(savepath, "w"));
        cassure(fwrite(fileBuf, 1, fileBufSize, f) == fileBufSize);
    }
    
error:
    safeFree(fileBuf);
    if (f) fclose(f);
    return err;
}

void fragmentzip_close(fragmentzip_t *info){
    if (info){
        safeFree(info->url);
        safeFreeCustom(info->mcurl, curl_easy_cleanup);
        safeFree(info->internal.cd_end);
        safeFree(info->cd);
        safeFreeCustom(info->localFile, fclose);
        safeFree(info->internal.cd64_end_locator);
        safeFree(info->internal.cd64_end);
        safeFree(info);
    }
}

const char* fragmentzip_version(){
    return VERSION_STRING;
}
