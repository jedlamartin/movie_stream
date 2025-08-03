#include "export.h"

void export(int movies_fd) {
  // Open html

  // Open movie dir
  DIR* dir = opendir("./movies");
  if (!dir) {
    char msg[] = "Could not open the movie directory!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  const char header[] =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "<title>Movie Streamer</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Movies</h1>\n"
      "<ul>\n";
  if (write(movies_fd, header, strlen(header)) < 0) {
    char msg[] = "Could not write!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  struct dirent* dir_entry = NULL;

  while ((dir_entry = readdir(dir)) != NULL) {
    if (strcmp(".", dir_entry->d_name) != 0 &&
        strcmp("..", dir_entry->d_name) != 0) {
      write(movies_fd, "<li><a href=\"stream?movie=", 26);
      write(movies_fd, dir_entry->d_name, strlen(dir_entry->d_name));
      write(movies_fd, "\">", 2);
      write(movies_fd, dir_entry->d_name, strlen(dir_entry->d_name));
      write(movies_fd, "</li>\n", 6);
    }
  }

  closedir(dir);

  const char footer[] =
      "</ul>\n"
      "</body>\n"
      "</html>";

  write(movies_fd, footer, strlen(footer));
  lseek(movies_fd, 0, SEEK_SET);
}
