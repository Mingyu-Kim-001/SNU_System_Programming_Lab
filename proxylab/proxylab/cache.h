#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct{
	char* hostname;
	char* pathname;
	char* data;
	int size;
	int port;
	struct CachedItem *prev;
	struct CachedItem *next;
} CachedItem;

extern volatile int readcnt;
extern sem_t mutex, w;
void cache_init();
void insert_cache(CachedItem* c);
void remove_LRU();
int isSame(CachedItem *c,char *hostname, int port, char *pathname);
CachedItem *find_cache(char *hostname, int port, char *pathname);
CachedItem *create_CI(char *hostname,char *port, char *pathname, char *data);
void printCachedItems();
