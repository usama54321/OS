void hijack_start(void *, void* new);
void hijack_pause ( void *target );
void hijack_resume ( void *target );
void hijack_stop ( void *target );
inline void restore_wp ( unsigned long cr0 );
inline unsigned long disable_wp ( void );
