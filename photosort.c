#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
//#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include "config.h"

/* ^^^^^^^^^^^^^^^^^ photosort.c ^^^^^^^^^^^^^^^^^
Copy photos from an external drive and sort them
by date into a predetermined folder in the computer.

This program takes as input a .txt file path on which the source folder and
the photo file names are written. Executing photosort.sh automatically creates
the required .txt file, then runs the compiled version of this script.

Enter "photosort --help" more info.
*/

enum ErrorCodes {
	ERR_FILE_R = 1,
	ERR_PH_NAME_BIG,
	ERR_PATH_NAME_BIG,
	ERR_ACCESSING_PH_IN_DEV,
	ERR_PH_TIME,
	ERR_CREATING_FOLDER,
	ERR_HANDLING_FOLDER,
	ERR_FILE_W
};


// ============================= OBJECTS ============================= 

#define PATH_LEN 100
typedef struct FileContent {
	char path[PATH_LEN];
	char **photos;
	size_t len;
} FileContent;

typedef struct List {
	char **elem;
	int len;
} List;


// ========================== WRAPPER FUNCTIONS ============================

void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
		exit(1);
	}
	return ptr;
};

void *xcalloc(size_t nmemb, size_t size) {
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory allocating %zu elements of %zu bytes\n", nmemb, size);
		exit(1);
	}
	return ptr;
};


// ============================= FileContent FUNCTIONS ============================= 

/* The allocation of fc->photos[i] is left to the caller. */
FileContent *createFileContent(size_t n_photos) {
	FileContent *fc = xmalloc(sizeof(*fc));
	fc->photos = xcalloc(n_photos, sizeof(char *));
	fc->len = n_photos;
	return fc;
};

void destroyFileContent(FileContent *fc) {
	for (size_t i=0; i < fc->len; i++) {
		if (fc->photos[i]) {free(fc->photos[i]); fc->photos[i] = NULL;}
	}
	if (fc->photos) {free(fc->photos); fc->photos = NULL;}
	if (fc) {free(fc); fc = NULL;}
};


// =================================== List FUNCTIONS ==================================

List *createList(size_t n_photos) {
	List *l = xmalloc(sizeof(*l));
	l->elem = xcalloc(n_photos, sizeof(char *));
	l->len = 0;
	return l;
};

void destroyList(List *l) {
	for (int i=0; i < l->len; i++) {
		if (l->elem[i]) {free(l->elem[i]); l->elem[i] = NULL;}
	}
	if (l->elem) {free(l->elem); l->elem = NULL;}
	if (l) {free(l); l = NULL;}
};

// Set "entry" as the last element of the list "l".
// RETURN VALUES: 0 if no errors occurred, -1 otherwise.
int append(List *l, const char *entry) {
	char *copy = strdup(entry);
	if (!copy) return -1;
	l->elem[l->len++] = copy;
	return 0;
}

void removeLast(List *l) {
	assert(l->len >= 0);
	if (l->len == 0) return;

	free(l->elem[l->len - 1]);
	l->elem[l->len - 1] = NULL;
	l->len--;
};

void printList(List *l) {
	for (int i=0; i < l->len; i++) {
		printf("\t%s\n", l->elem[i]);
	}
};

// =========================== FUNCTIONS ================================

#define PHOTONAME_BUF_SIZE 100
size_t countPhotos(FILE *fp) {
	size_t counter = 0;
	void *check;

	do {
		char buf[PHOTONAME_BUF_SIZE];
		check = fgets(buf, sizeof(buf), fp);
		counter++;
	} while (check);
	
	return counter - 2;
};

void copyBufferToFileContent(char *destination, char *source) {
	source[strcspn(source, "\n")] = 0;
	strcpy(destination, source);
};

// Compose photo's path (fc->path + "/" + fc->photos[i]) 
void composePhotoPath(const char* path, const char *photo, char* buf) {
	buf[0] = 0;
	strcat(strcat(strcat(buf, path), "/"), photo);
};

#define HOME "/home/"
#define MARK "-PHOTOSORT/"
// Compose saving destination folder
void composeDestinationFolder(const char *user, const char *date, char *buf) {
	buf[0] = 0;
	strcat(strcat(strcat(strcat(strcat(buf, HOME), user), SAVE_LOCATION), date), MARK);	
};

void composeDestinationPath(const char *folder, const char *photo, char *buf) {
	buf[0] = 0;
	strcat(strcat(buf, folder), photo);
};

// =============================== MAIN ======================================

#define HELP_OPTION "--help"
#define REMOVE_OPTION "-r"
#define PATHNAME_BUF_SIZE 100
#define USER_MAX_LEN 20
#define MAX_DATE_LEN 11
#define USER_START 7
#define CHUNK_SIZE (1024 * 8) // 8 KiB
int main(int argc, char **argv) {
	int status = 0;
	int isRemove = 0;
	int sorted = 0;
	int removing_error = 0;
	FileContent *fc = NULL;
	List *skipped = NULL;
	FILE *fp = NULL;

	
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename> [<option>]\n", argv[0]);
		fprintf(stderr, "Enter \"%s --help\" for more info.\n", argv[0]);
		exit(1);

	}
	if (strcmp(argv[1], HELP_OPTION) == 0 || (argc > 2 && strcmp(argv[2], HELP_OPTION) == 0)) {
		printf("Usage: %s <filename> [<option>]\n", argv[0]);

		printf("Options:\n");
		printf("\t\"%s\": print this screen.\n\n", HELP_OPTION);
		printf("\t\"%s\":\tremove files from source media storage device\n",  REMOVE_OPTION);
		printf("\t\tafter sorting them.\n");

		goto ClosingSequence;

	} else if (argc > 2 && strcmp(argv[2], REMOVE_OPTION) == 0)	{isRemove = 1;}


	// ----- COPY DATA FROM FILE TO MEMORY -----

	fp = fopen(argv[1], "r");
	if (!fp) {status = ERR_FILE_R; goto ClosingSequence;}

	size_t n_photos = countPhotos(fp);
	fc = createFileContent(n_photos);

	rewind(fp);


	// ----- SAVE PATH NAME -----

	char pathbuf[PATHNAME_BUF_SIZE];
	void *check = fgets(pathbuf, sizeof(pathbuf), fp);
	if (!check) {status = ERR_PH_NAME_BIG; goto ClosingSequence;}
	copyBufferToFileContent(fc->path, pathbuf);
	

	// ----- SAVE PHOTOS' NAMES -----

	for (size_t i=0; i < n_photos; i++) {
		char buf[PHOTONAME_BUF_SIZE];
		void *check = fgets(buf, sizeof(buf), fp);

		if (!check) {status = ERR_PATH_NAME_BIG; goto ClosingSequence;}

		fc->photos[i] = xmalloc(sizeof(buf)); // allocate photo
		copyBufferToFileContent(fc->photos[i], buf);
	}

	fclose(fp);
	fp = NULL;

	
	// ----- COPY PHOTOS FROM PATH TO DIRECTORIES IN PC -----

	// Retrieve user's name
	printf("%s\n", fc->path);
	char user[USER_MAX_LEN];
	int i=0;

	while (i < USER_MAX_LEN - 1 && fc->path[USER_START + i] != '/' && fc->path[USER_START + i] != 0) {
		user[i] = fc->path[USER_START + i]; // Skip "/media/"
		i++;
	}
	user[i] = 0;


	skipped = createList(n_photos);
	for (size_t i=0; i < fc->len; i++) {
		char *path = fc->path;
		char *photo = fc->photos[i];

		char src_path[strlen(path) + 1 + strlen(photo) + 1];

		// Compose photo's path (fc->path + "/" + fc->photos[i]) 

		composePhotoPath(path, photo, src_path);

		// --- Retrieve photo's date ---

		struct stat ph_st;

		int check_stat = stat(src_path, &ph_st);
		if (check_stat == -1) {status = ERR_ACCESSING_PH_IN_DEV; goto ClosingSequence;}
		
		char date[MAX_DATE_LEN];
		date[0] = 0;
		size_t check_strftime = strftime(date, MAX_DATE_LEN, "%Y%m%d", localtime(&ph_st.st_mtime));
		if (check_strftime == 0) {status = ERR_PH_TIME; goto ClosingSequence;}


		// --- Copy photo into destination folder --- //FIXME

		// Retrieve destination folder's name
		char folder[strlen(HOME) + strlen(user) + strlen(SAVE_LOCATION) \
					+ strlen(date) + strlen(MARK) + 1];
		composeDestinationFolder(user, date, folder);

		// Create/Check folder
		if (mkdir(folder, 0755) != 0) {
			if (errno != EEXIST) {status = ERR_CREATING_FOLDER; goto ClosingSequence;}
		}

		// Check photo
		char dest_path[strlen(folder) + strlen(photo) + 1];
		composeDestinationPath(folder, photo, dest_path);

		FILE *dest_fp = fopen(dest_path, "wbx");
		if (!dest_fp) {
			if (errno == EEXIST) continue; // Photo already exists
			status = ERR_FILE_W; goto ClosingSequence;
		} 
		// FIXME: different photos with identical names count as the same 

		FILE *src_fp = fopen(src_path, "rb");
		if (!src_fp) {status = ERR_FILE_R; goto ErrorHandling;}

		long byte_counter = 0;
		char chunk[CHUNK_SIZE];
	
		size_t bytes_read;
		while ((bytes_read = fread(chunk, 1, CHUNK_SIZE, src_fp)) > 0) {
			if (fwrite(chunk, 1, bytes_read, dest_fp) != bytes_read) {
				status = 255; goto ErrorHandling; //FIXME: usa un errore apposito
			}
			byte_counter += bytes_read;
		}

		// Check for errors during reading/writing
		if (ferror(src_fp) != 0) {status = ERR_FILE_R; goto ErrorHandling;}
		else {fclose(src_fp); src_fp = NULL;}

		if (byte_counter != ph_st.st_size || ferror(dest_fp) != 0) {
			append(skipped, photo);
			status = ERR_FILE_W; goto ErrorHandling;	
		}

		if (fclose(dest_fp) != 0) {
			dest_fp = NULL;
			status = ERR_FILE_W; goto ErrorHandling;
		}
		dest_fp = NULL;
		
		sorted++;

		// Remove (if needed)
		if (isRemove == 1) {
			if (remove(src_path) == -1 && removing_error == 0) {removing_error = 1;}
		} 
ErrorHandling:
		if (src_fp) fclose(src_fp);
		if (dest_fp) fclose(dest_fp);
		if (status != 0) {
			remove(dest_path);
			goto ClosingSequence;
		}
	} // for (photos in FileContent) --> copy

	// ----- Print useful data before closing -----
ClosingSequence:		
	if (fc) {
		printf("------------------------\n");
		printf("Photos: sorted %d, skipped %zu\n", sorted, (fc->len - sorted));
	}
	if (skipped && skipped->len != 0) {
		printf("Skipped:\n");
		printList(skipped);
	}
	if (removing_error == 1) {fprintf(stderr, "[!!!] Error removing photo(s) from media storage device.\n");}


	// ---- CLOSING SEQUENCE -----
	if (fc) destroyFileContent(fc);
	if (fp) fclose(fp);
	if (skipped) destroyList(skipped);

	if (status != 0) fprintf(stderr, "[!!!] ");
	switch (status) {
	case 0: return 0;
	case ERR_FILE_R:
		fprintf(stderr, "Error while reading a file.\n");
		exit(1);
	case ERR_PH_NAME_BIG: 
		fprintf(stderr, "Error: a photo's name is too big.\n");
		exit(1);
	case ERR_PATH_NAME_BIG: 
		fprintf(stderr, "Error: the path's name is too big.\n");
		exit(1);
	case ERR_ACCESSING_PH_IN_DEV: 
		fprintf(stderr, "Error accessing photo in the media storage device.\n");
		exit(1);
	case ERR_PH_TIME: 
		fprintf(stderr, "Error retrieving photo's time.\n");
		exit(1);
	case ERR_CREATING_FOLDER:
		fprintf(stderr, "Error creating folder.\n");
		exit(1);
	case ERR_HANDLING_FOLDER:
		fprintf(stderr, "Error handling a folder.\n");
		exit(1);
	case ERR_FILE_W:
		fprintf(stderr, "Error while writing to a file.\n");
		exit(1);

	default:
		fprintf(stderr, "Error.\n");
		exit(1);
	} 
}
