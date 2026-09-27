#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include "config.h"

/* ^^^^^^^^^^^^^^^^^ photosort.c ^^^^^^^^^^^^^^^^^
Copy photos from an external drive and sort them
by date into a predetermined folder in the computer.

This program takes as input a .txt file path on which the source folder and
the photo file names are written. Executing photosort.sh automatically creates
the required .txt file, then runs the compiled version of this script.

Enter "photosort --help" more info.
*/


// ============================= OBJECTS ============================= 

#define PATH_LEN 40
typedef struct FileContent {
	char path[PATH_LEN];
	char **photos;
	size_t len;
} FileContent;


// ========================== WRAPPER FUNCTIONS ============================

void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory allocating %zu bytes", size);
		exit(1);
	}
	return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory allocating %zu elements of %zu bytes", nmemb, size);
		exit(1);
	}
	return ptr;
}


// ============================= OBJECT FUNCTIONS ============================= 

#define PHOTONAME_BUF_SIZE 50
/* The allocation of fc->photos[i] is left to the caller. */
FileContent *createFileContent(size_t n_photos) {
	FileContent *fc = xmalloc(sizeof(*fc));
	fc->photos = xcalloc(n_photos, sizeof(char *));
	fc->len = n_photos;
	return fc;
};

void allocatePhoto(FileContent *fc, int idx, size_t size) {
	fc->photos[idx] = xmalloc(size);
};

void destroyFileContent(FileContent *fc) {
	for (size_t i=0; i < fc->len; i++) {
		if (fc->photos[i]) {free(fc->photos[i]); fc->photos[i] = NULL;}
	}
	if (fc) {free(fc); fc = NULL;}
}

// =========================== FUNCTIONS ================================

int countPhotos(FILE *fp) {
	int counter = 0;
	void *check;

	do {
		char buf[PHOTONAME_BUF_SIZE];
		check = fgets(buf, sizeof(buf), fp);
		counter++;
	} while (check);
	
	return counter - 2;
};

void copyBufferToFileContent(char *destination, char *source) {	
	memcpy(destination, source, strlen(source));
	destination[strlen(source) - 1] = 0;
};

// Compose photo's path (fc->path + "/" + fc->photos[i]) 
void composePhotoPath(const char* path, const char *photo, char* buf) {
	buf[0] = 0;
	strcat(strcat(strcat(buf, path), "/"), photo);
};

#define HOME "/home/"
#define MARK " - PHOTOSORT"
// Compose saving destination folder (/home/ + $USER + /Pictures/macchinetta/ + $DATE)
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
int main(int argc, char **argv) {
	char *path = argv[1];
	int status = 0;
	int remove = 0;
	FileContent *fc = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename> [<option>]\n", argv[0]);
		fprintf(stderr, "Enter \"%s --help\" for more info\n.", argv[0]);
		exit(1);
	}

	if (strcmp(path, HELP_OPTION) == 0) {
		printf("Usage: %s <filename> [<option>]\n", argv[0]);

		printf("Options:\n");
		printf("\t\"--help\": print this screen.\n\n");
		printf("\t\"-%s\":\tremove files from source media storage device\n",  REMOVE_OPTION);
		printf("\t\tafter sorting them.\n");

		goto ClosingSequence;

	} else if (strcmp(argv[2], REMOVE_OPTION) == 0)	{remove = 1;}


	// ----- COPY DATA FROM FILE TO MEMORY -----

	FILE *fp = fopen(path, "r");
	if (!fp) {status = 1; goto ClosingSequence;}

	size_t n_photos = countPhotos(fp);
	fc = createFileContent(n_photos);

	rewind(fp);


	// ----- SAVE PATH NAME -----

	char pathbuf[PATHNAME_BUF_SIZE];
	void *check = fgets(pathbuf, sizeof(pathbuf), fp);
	if (!check) {status = 2; goto ClosingSequence;}
	copyBufferToFileContent(fc->path, pathbuf);
	

	// ----- SAVE PHOTOS' NAMES -----

	for (size_t i=0; i < n_photos; i++) {
		char buf[PHOTONAME_BUF_SIZE];
		void *check = fgets(buf, sizeof(buf), fp);

		if (!check) {status = 3; goto ClosingSequence;}

		allocatePhoto(fc, i, sizeof(buf));
		copyBufferToFileContent(fc->photos[i], buf);
	}

	fclose(fp);
	fp = NULL;

	
	// ----- COPY PHOTOS FROM PATH TO DIRECTORIES IN PC -----

	// Retrieve user's name
	printf("%s\n", fc->path);
	char user[USER_MAX_LEN];
	int i=0;

	while (fc->path[USER_START + i] != '/') {
		user[i] = fc->path[USER_START + i]; // Skip "/media/"
		i++;
	}
	user[i] = 0;

	for (size_t i=0; i < fc->len; i++) {
		char *path = fc->path;
		char *photo = fc->photos[i];
		char src_path[strlen(path) + 1 + strlen(photo) + 1];

		// Compose photo's path (fc->path + "/" + fc->photos[i]) 

		composePhotoPath(path, photo, src_path);

		// Retrieve photo's date

		struct stat phstat;

		int check_stat = stat(src_path, &phstat);
		if (check_stat == -1) {status = 4; goto ClosingSequence;}
		
		char date[MAX_DATE_LEN];
		date[0] = 0;
		size_t check_strftime = strftime(date, MAX_DATE_LEN, "%Y%m%d", \
										 localtime(&phstat.st_mtim.tv_sec));
		if (check_strftime == 0) {status = 5; goto ClosingSequence;}

		printf("%s\n", date); //debug 

		// --- Copy photo into "~/Pictures/macchinetta/$date" ---

		// Retrieve destination folder's name
		char folder[strlen(HOME) + strlen(user) + strlen(SAVE_LOCATION) \
					+ strlen(date) + strlen(MARK) + 1];
		composeDestinationFolder(user, date, folder);

		// Don't know if this will ever be used 
		char destination_path[strlen(folder) + strlen(fc->photos[i]) + 1];
		composeDestinationPath(folder, fc->photos[i], destination_path);

		// Copy photo into destination folder
		//TODO:
		/*
		int sorted=0;
		int removing_error = 0;
		if (photo) {
			if (!) create folder 
			if (error creating folder) {status = 6; goto ClosingSequence;}
			if (photo already in folder) {continue;}
			save photo in destination path
			if (error) {
				add photo name to skipped
				continue;
			}
			
			sorted++;

			if (remove == 1) {
				remove photo from sd
				if (error removing photo) {removing_error = 1;}
			}
		}
		printf("Photos: sorted %s, skipped %d\n", sorted, fc->len - sorted);
		if (removing_error = 1) {printf("Error removing photo(s) from media storage device.\n");}
		print list of skipped photos*/


	}


ClosingSequence:
	// ---- CLOSING SEQUENCE -----
	if (fc) destroyFileContent(fc);
	if (fp) fclose(fp);

	switch (status) {
	case 0: return 0;
	case 1:
		fprintf(stderr, "Error with a file.\n");
		exit(1);
	case 2: 
		fprintf(stderr, "Error: a photo's name is too big.\n");
		exit(1);
	case 3: 
		fprintf(stderr, "Error: the path's name is too big.\n");
		exit(1);
	case 4: 
		fprintf(stderr, "Error accessing photo in the media storage device.\n");
		exit(1);
	case 5: 
		fprintf(stderr, "Error retrieving photo's time.\n");
		exit(1);
	case 6:
		fprintf(stderr, "Error creating folder.\n");
		exit(1);

	default:
		fprintf(stderr, "Error.\n");
		exit(1);
	} 
}
