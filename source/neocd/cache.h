#ifdef __cplusplus
extern "C" {
#endif
int file_cache(char *filename, int offset, int size,int type);
int find_spec(char *spec, char *name, uint *offset, uint *size);
void restore_override();
void get_cache_origin(int type, int offset, char **name, int *nb);
void put_override(int type, char *name, uint size_msg); 
void clear_file_cache();
void cache_set_crc(int offset,int size,int type);
void prepare_cdda_save(UINT32 id);
void prepare_cache_save();

#ifdef __cplusplus
}
#endif
