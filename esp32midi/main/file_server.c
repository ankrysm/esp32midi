/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "local.h"

#define HTML_TITLE "ESP32-Midi-Server"
#define HTML_HEADER "<!DOCTYPE html><html><meta charset=\"UTF-8\"><head><title>"HTML_TITLE"</title></head><body bgcolor=\"white\">"
#define HTML_FOOTER "</body></html>"

#define INDEX_HTML "index.html"
#define FILES_HTML "files.html"
#define SETTINGS_HTML "settings.html"

#define DOWNLOAD_PATH "downloads"
#define PLAY_PATH "play"
#define PLAY_RANDOM_PATH "playrandom"
#define DELETE_PATH "delete"
#define STOP_PATH "stop"
#define UPLOAD_PATH "upload"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "file_server";

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}


/**
 * *** MAIN-HTML-Page ****
 *
 * Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories
 * /
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    // Retrieve the base path of file storage to construct the full path
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        // Respond with 404 Not Found
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    // Send HTML file header
    httpd_resp_sendstr_chunk(req, HTML_HEADER);

    // Get handle to embedded file upload script
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    // Add file upload form and script which on execution sends a POST request to /upload
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    // Send file-list table definition and column labels
#ifdef WITH_PRINING_MIDIFILES
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th><th>Play</th><th>Print</th></tr></thead>"
        "<tbody>");
#else
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th><th>Play</th></tr></thead>"
        "<tbody>");
#endif

    // Iterate over all files / folders and fetch their names and sizes
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        // Send chunk of HTML file containing table entries with file name and size
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);

        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");

        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/play");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Play</button></form>");
#ifdef WITH_PRINING_MIDIFILES
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/print");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Print</button></form>");
#endif
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    // Line with file system info and stop button
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    char fsinfo[32];
    snprintf(fsinfo, sizeof(fsinfo),"%d / %d", used,total);
    httpd_resp_sendstr_chunk(req, "<tr><td>Total</td><td>Filesystem</td><td>");
    httpd_resp_sendstr_chunk(req, fsinfo);
    httpd_resp_sendstr_chunk(req, "</td><td></td><td><form method=\"post\" action=\"/stop\">");
    httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Stop</button></form></td>");
#ifdef WITH_PRINING_MIDIFILES
    httpd_resp_sendstr_chunk(req, "<td> </td><td> </td></tr>\n");
#else
    httpd_resp_sendstr_chunk(req, "</tr>\n");
#endif

#ifdef WITH_PRINING_MIDIFILES
    httpd_resp_sendstr_chunk(req, "<td> </td><td> </td></tr>\n");
#else
    httpd_resp_sendstr_chunk(req, "</tr>\n");
#endif

    // Finish the file list table
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    // FURTHER BUTTONS
    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/playrandom\">");
    httpd_resp_sendstr_chunk(req, "<button type=\"submit\">Play Random</button></form></td>");

    httpd_resp_sendstr_chunk(req, "<form method=\"get\" action=\"/setvol\">Volume:<input type=\"text\" name=\"volume\" value=\"100\">");
    httpd_resp_sendstr_chunk(req, "<input type=\"submit\" value=\"Set\"></form>");

    // Send remaining chunk of HTML file to complete it
    httpd_resp_sendstr_chunk(req, "</body></html>");

    // Send empty chunk to signal HTTP response completion
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}
*/

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        // Full path string won't fit into destination buffer
        return NULL;
    }

    // Construct full path (base + path)
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    // Return pointer to path, skipping the base
    return dest + base_pathlen;
}

/**
 * play random
 */
static esp_err_t playrandom_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "%s: Play something random %s",__func__, req->uri);

    // play with delay
    handle_play_random_midifile(((struct file_server_data *)req->user_ctx)->base_path, 1 );

    // Redirect onto root to see the file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/"FILES_HTML);
    httpd_resp_sendstr(req, "Start play random successfully");

    return ESP_OK;
}

// XXX
//static esp_err_t setvol50_post_handler(httpd_req_t *req)
//{
//    ESP_LOGI(TAG, "setvol 50 %s",req->uri);
//
//    midi_volume(0x10);
//
//    play_ok();
//
//    // Redirect onto root to see the file list
//    httpd_resp_set_status(req, "303 See Other");
//    httpd_resp_set_hdr(req, "Location", "/");
//    httpd_resp_sendstr(req, "Start play random successfully");
//
//    return ESP_OK;
//}

//static esp_err_t setvol100_post_handler(httpd_req_t *req)
//{
//    ESP_LOGI(TAG, "setvol 100 %s",req->uri);
//
//    midi_volume(0x7f);
//
//    play_ok();
//
//    // Redirect onto root to see the file list
//    httpd_resp_set_status(req, "303 See Other");
//    httpd_resp_set_hdr(req, "Location", "/");
//    httpd_resp_sendstr(req, "Start play random successfully");
//
//    return ESP_OK;
//}

/**
 * Handler Stop playing
 */
static esp_err_t stop_playing_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Stop Playing %s",req->uri);

    handle_stop_midifile();

    // Redirect onto root to see the file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/"FILES_HTML);
    httpd_resp_sendstr(req, "Stop playing successfully");

    return ESP_OK;
}

/**
 * GET-Handler
 */
static esp_err_t get_handler_index_html(httpd_req_t *req)
{
//    httpd_resp_set_status(req, "307 Temporary Redirect");
//    httpd_resp_set_hdr(req, "Location", "/");
//    httpd_resp_send(req, NULL, 0);  // Response body can be empty

    ESP_LOGI(TAG, "get_handler_index_html '%s'", req->uri);

    // Send HTML file header
    httpd_resp_sendstr_chunk(req, HTML_HEADER);

    // HTML content
    httpd_resp_sendstr_chunk(req,
    		"<center><h1>Welcome to ESP32-Midi-Server</h1></center>" \
			"<hr>" \
			"<center>" \
			"<h2>Hauptseite</h2>" \
			"<p><a href=\""FILES_HTML"\">Dateien</a></p>" \
			"<p><a href=\""SETTINGS_HTML"\">Einstellungen</a></p>" \
			"</center>");

    // Footer
    httpd_resp_sendstr_chunk(req, HTML_FOOTER);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

/*
 * response for files.html
 *
 * with path: files.html?path=<path>
 */
static esp_err_t get_handler_files_html(httpd_req_t *req) {

    char dirpath[FILE_PATH_MAX];
//    FILE *fd = NULL;
//    struct stat file_stat;
    const char *dir_from_uri = strlen(req->uri) < strlen(FILES_HTML) ? "" : (req->uri) + strlen(FILES_HTML)+1;

    ESP_LOGI(TAG, "get_handler_files_html '%s' (%s)", req->uri,dir_from_uri);

    const char *filename = get_path_from_uri(dirpath, ((struct file_server_data *)req->user_ctx)->base_path, dir_from_uri, sizeof(dirpath));
    if (!filename) {
    	ESP_LOGE(TAG, "Filename is too long");
    	// Respond with 500 Internal Server Error
    	httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    	return ESP_FAIL;
    }

//    // If name has trailing '/', respond with directory contents
//    if (filename[strlen(filename) - 1] == '/') {
//        return http_resp_dir_html(req, filepath);
//    }


//    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "get_handler_files_html NYI");
//    return ESP_FAIL;

//    char *dirpath = BASE_PATH; // "/spiffs"
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);

    // Retrieve the base path of file storage to construct the full path
    strlcpy(entrypath, dirpath, sizeof(entrypath));
    strlcat(entrypath, "/", sizeof(entrypath)); // entrypath is "/spiffs/"
    const size_t dirpath_len = strlen(entrypath);

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        // Respond with 404 Not Found
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "dir '%s' open", dirpath);

    // Send HTML file header
    httpd_resp_sendstr_chunk(req, HTML_HEADER);

    // Get handle to embedded file upload script
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    // Add file upload form and script which on execution sends a POST request to /upload
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    // Send file-list table definition and column labels
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th><th>Play</th></tr></thead>"
        "<tbody>");

    int file_cnt=0;
    // Iterate over all files / folders and fetch their names and sizes
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entrypath);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        /* Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\""DOWNLOAD_PATH"/");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);

        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete/");
        //httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");

        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/play/");
        //httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Play</button></form>");
        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
        file_cnt++;
    }
    closedir(dir);

    if ( ! file_cnt) {
        httpd_resp_sendstr_chunk(req, "<tr><td>no files</td><td/><td/><td/></td></tr>\n");

    }
    // Line with file system info and stop button
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    char fsinfo[32];
    snprintf(fsinfo, sizeof(fsinfo),"%d / %d", used,total);
    httpd_resp_sendstr_chunk(req, "<tr><td>Total</td><td>Filesystem</td><td>");
    httpd_resp_sendstr_chunk(req, fsinfo);

    httpd_resp_sendstr_chunk(req, "</td></td></td></tr>\n");

    // Finish the file list table
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    httpd_resp_sendstr_chunk(req,  "<p><form method=\"post\" action=\"/stop\"><button type=\"submit\">Stop</button></form></p>");
    httpd_resp_sendstr_chunk(req,  "<p><form method=\"post\" action=\"/playrandom\"><button type=\"submit\">Play Random</button></form></p>");
    httpd_resp_sendstr_chunk(req,  "<p><form method=\"get\" action=\"/settings.html\">Lautstärke:<input type=\"text\" name=\"volume\" value=\"100\"><input type=\"submit\" value=\"Set\"></form></p>");

    httpd_resp_sendstr_chunk(req,  "<p><a href=\"/index.html\">Startseite</a></p>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, HTML_FOOTER);

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;

}

static esp_err_t get_handler_settings_html(httpd_req_t *req) {
    ESP_LOGI(TAG, "get_handler_files_html '%s'", req->uri);

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "get_handler_files_html NYI");
    return ESP_FAIL;

}



/**
 *  Handler to download a file kept on the server
 */
static esp_err_t get_handler_download_html(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    ESP_LOGI(TAG, "%s: GET '%s'", __func__, req->uri);

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path, req->uri + sizeof("/"DOWNLOAD_PATH) - 1, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s: download '%s'", __func__, filename);

    // If name has trailing '/', respond with directory contents
    //if (filename[strlen(filename) - 1] == '/') {
    //    return http_resp_dir_html(req, filepath);
    //}

    if (stat(filepath, &file_stat) == -1) {
        // If file not present on SPIFFS check if URI
        // corresponds to one of the hardcoded paths
        //if (strcmp(filename, "/index.html") == 0) {
        //    return index_html_get_handler(req);
//        } else if (strcmp(filename, "/favicon.ico") == 0) {
//            return favicon_get_handler(req);
//        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        // Respond with 404 Not Found
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    // Retrieve the pointer to scratch buffer for temporary storage
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        // Read file in chunks into the scratch buffer
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        // Send the buffer contents as HTTP response chunk
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
            fclose(fd);
            ESP_LOGE(TAG, "File sending failed!");
            // Abort sending file
            httpd_resp_sendstr_chunk(req, NULL);
            // Respond with 500 Internal Server Error
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }

        // Keep looping till the whole file is sent
    } while (chunksize != 0);

    // Close file after sending complete
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    // Respond with an empty chunk to signal HTTP response completion
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
// */

static esp_err_t common_get_handler(httpd_req_t *req) {

	ESP_LOGI(TAG, "%s: GET '%s'", __func__, req->uri);

	if (!req->uri) {
		ESP_LOGE(TAG, "no req->uri");
		// Respond with 500 Internal Server Error
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "wrong or missing URL");
		return ESP_FAIL;
	}

	if (strcmp(req->uri,"/") == 0 || ! strlen(req->uri)) {
		return get_handler_index_html(req);
	} else if (strcmp(req->uri, "/"INDEX_HTML) == 0) {
		return get_handler_index_html(req);
	} else if (strcmp(req->uri, "/favicon.ico") == 0) {
		return favicon_get_handler(req);
	} else if (strstr(req->uri, "/"FILES_HTML) == req->uri) {
		return get_handler_files_html(req);
	} else if (strstr(req->uri, "/"DOWNLOAD_PATH"/") == req->uri) {
		return get_handler_download_html(req);
	} else if (strcmp(req->uri, "/"SETTINGS_HTML) == 0) {
		return get_handler_settings_html(req);
	}
	ESP_LOGE(TAG, "%s: no handler for '%s'", __func__, req->uri);
	/* Respond with 404 Not Found */
	httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Here's something wrong");
	return ESP_FAIL;
}


/**
 *  Handler to upload a file onto the server
 */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    // Skip leading "/upload" from URI to get filename
    // Note sizeof() counts NULL termination hence the -1
    const char *filename = get_path_from_uri(
    		filepath,
			((struct file_server_data *)req->user_ctx)->base_path,
			req->uri + sizeof("/"UPLOAD_PATH) - 1,
			sizeof(filepath)
    );

    if (!filename) {
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    // Filename cannot have a trailing '/'
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "%s: Invalid filename : %s", __func__, filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "%s: File already exists : %s", __func__, filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    // File cannot be larger than a limit
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "%s: File too large : %d bytes", __func__, req->content_len);
        // Respond with 400 Bad Request
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        // Return failure to close underlying connection else the
        // incoming file content will keep the socket busy
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "%s: Failed to create file : %s", __func__, filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s: Receiving file : %s...", __func__, filename);

    // Retrieve the pointer to scratch buffer for temporary storage
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    // Content length of the request gives
    // the size of the file being uploaded
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "%s: Remaining size : %d", __func__, remaining);
        // Receive the file part by part into a buffer
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry if timeout occurred
                continue;
            }

            // In case of unrecoverable error,
            // close and delete the unfinished file
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "%s: File reception %s failed!", __func__, filename);
            // Respond with 500 Internal Server Error
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        // Write buffer content to file on storage
        if (received && (received != fwrite(buf, 1, received, fd))) {
            //Couldn't write everything to file!
            // Storage may be full?
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "%s: File write %s failed!", __func__, filename);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        // Keep track of remaining size of
        // the file left to be uploaded
        remaining -= received;
    }

    // Close file upon upload completion
    fclose(fd);
    ESP_LOGI(TAG, "%s: File %s reception complete",__func__, filename);

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/"FILES_HTML);
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/**
 *  Handler to delete a file from the server
 */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    // Skip leading "/delete" from URI to get filename */
    // Note sizeof() counts NULL termination hence the -1
    const char *filename = get_path_from_uri(
    		filepath,
			((struct file_server_data *)req->user_ctx)->base_path,
			req->uri + sizeof("/"DELETE_PATH) - 1,
			sizeof(filepath));
    if (!filename) {
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    // Filename cannot have a trailing '/'
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "%s: Invalid filename : %s", __func__, filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "%s: File does not exist : %s", __func__, filename);
        // Respond with 400 Bad Request
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s: Deleting file : %s", __func__, filename);
    // Delete file
    unlink(filepath);

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/"FILES_HTML);
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

/**
 *  Handler to play a midifile
 */
static esp_err_t play_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    // Skip leading "/play" from URI to get filename
    // Note sizeof() counts NULL termination hence the -1
    const char *filename = get_path_from_uri(
    		filepath,
			((struct file_server_data *)req->user_ctx)->base_path,
			req->uri + sizeof("/"PLAY_PATH) - 1,
			sizeof(filepath));
    if (!filename) {
        // Respond with 500 Internal Server Error
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    // Filename cannot have a trailing '/'
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "%s: Invalid filename : %s", __func__, filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "%s: File does not exist : %s", __func__, filename);
        // Respond with 400 Bad Request
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s: play file : %s at %s", __func__, filename, filepath);

    // play with delay
    if ( handle_play_midifile(filepath, 1)) {
    	play_err();
        ESP_LOGE(TAG, "not a valid midi file : %s", filename);
         // Respond with 400 Bad Request
         httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not a valid MIDI-File");
         return ESP_FAIL;
    }

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/"FILES_HTML);
    httpd_resp_sendstr(req, "Start play successfully");
    return ESP_OK;
}

#ifdef WITH_PRINING_MIDIFILES
/**
 *  Handler to play a midifile
 */
static esp_err_t print_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    // Skip leading "/print" from URI to get filename
    // Note sizeof() counts NULL termination hence the -1
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/print") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    // Filename cannot have a trailing '/'
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        // Respond with 400 Bad Request
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "print file : %s at %s", filename, filepath);

    if ( handle_print_midifile(filepath)) {
    	play_err();
        ESP_LOGE(TAG, "not a valid midi file : %s", filename);
         // Respond with 400 Bad Request
         httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not a valid MIDI-File");
         return ESP_FAIL;
    }

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Printing successfully");
    return ESP_OK;
}
#endif

/**
 *  Function to start the file server
 */
esp_err_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    // Validate file storage base path
    if (!base_path || strcmp(base_path, BASE_PATH) != 0) {
        ESP_LOGE(TAG, "%s: File server presently supports only '"BASE_PATH"' as base path", __func__);
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE(TAG, "%s: File server already started",__func__);
        return ESP_ERR_INVALID_STATE;
    }

    // Allocate memory for server data
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "%s: Failed to allocate memory for server data",__func__);
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Use the URI wildcard matching function in order to
    // allow the same handler to respond to multiple different
    // target URIs which match the wildcard scheme
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "%s: Starting HTTP Server", __func__);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "%s: Failed to start file server!", __func__);
        return ESP_FAIL;
    }

//    httpd_uri_t setvol = {
//         .uri       = "/setvol",
//         .method    = HTTP_POST,
//         .handler   = setvol100_post_handler,
//         .user_ctx  = server_data    // Pass server data as context
//     };
//     httpd_register_uri_handler(server, &setvol);

    // URI handler for getting uploaded files
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = common_get_handler, //download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    // URI handler for uploading files to server
    httpd_uri_t file_upload = {
        .uri       = "/"UPLOAD_PATH"/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    // URI handler for deleting files from server
    httpd_uri_t file_delete = {
        .uri       = "/"DELETE_PATH"/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    // URI handler for playing a midifile
    httpd_uri_t file_play = {
        .uri       = "/"PLAY_PATH"/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = play_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_play);

#ifdef WITH_PRINING_MIDIFILES
    // URI handler for printing a midifile - serial monitor needed
     httpd_uri_t file_print = {
         .uri       = "/print/*",   // Match all URIs of type /delete/path/to/file
         .method    = HTTP_POST,
         .handler   = print_post_handler,
         .user_ctx  = server_data    // Pass server data as context
     };
     httpd_register_uri_handler(server, &file_print);
#endif

    // URI handler for stop playing
    httpd_uri_t stop_playing = {
        .uri       = "/"STOP_PATH,
        .method    = HTTP_POST,
        .handler   = stop_playing_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &stop_playing);

    // URI handler for playing random
    httpd_uri_t playrandom = {
        .uri       = "/"PLAY_RANDOM_PATH,
        .method    = HTTP_POST,
        .handler   = playrandom_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &playrandom);

    // URI handler for volume control
    //httpd_uri_t setvol50 = {
    //    .uri       = "/setvol50",
    //    .method    = HTTP_POST,
    //    .handler   = setvol50_post_handler,
    //    .user_ctx  = server_data    // Pass server data as context
    //};
    //httpd_register_uri_handler(server, &setvol50);

//    httpd_uri_t setvol100 = {
//         .uri       = "/setvol100",
//         .method    = HTTP_POST,
//         .handler   = setvol100_post_handler,
//         .user_ctx  = server_data    // Pass server data as context
//     };
//     httpd_register_uri_handler(server, &setvol100);

    return ESP_OK;
}
