//sudo apt-get install libssh-dev
//gcc ssh.c -o ssh.o -lssh
#include <stdio.h>
#include <libssh/libssh.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>

#define STRLEN 100

int run_remote_cmd(ssh_session session, char *cmd) {

  ssh_channel channel = ssh_channel_new(session);

  if (!channel)
    return SSH_ERROR;

  int result;
  char buffer[256];
  int nbytes;

  result = ssh_channel_open_session(channel);

  if (result != SSH_OK) {
    ssh_channel_free(channel);
    return result;
  }

  result = ssh_channel_request_exec(channel, cmd);

  if (result != SSH_OK) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return result;
  }

  nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);

  while (nbytes > 0) {
    if (write(1, buffer, nbytes) != nbytes) {
      ssh_channel_close(channel);
      ssh_channel_free(channel);
      return SSH_ERROR;
    }
    nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
  }

  if (nbytes < 0) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    return SSH_ERROR;
  }

  ssh_channel_send_eof(channel);
  ssh_channel_close(channel);
  ssh_channel_free(channel);

  return SSH_OK;
}



int main(int argc, char **argv) {

    if (argc < 3)
    	return -1;

    char username[STRLEN];
    const char *host = argv[1];
    int port = atoi(argv[2]);

    printf("User: ");
	fgets(username, STRLEN, stdin);
	username[strlen(username)-1] = '\0';

    ssh_session session = ssh_new();

    if (!session)
    	return -1;

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, username);

    int conn = ssh_connect(session);

    if (conn != SSH_OK) {
        printf("Error connecting to %s: %s\n", host, ssh_get_error(session));
    	return -1;
    } else {
        printf("Connected.\n");
    }

    int res;
    char cmd[STRLEN];
    char *password = getpass("Password: ");

    printf("Command: ");
	fgets(cmd, STRLEN, stdin);
	cmd[strlen(cmd)-1] = '\0';

    res = ssh_userauth_password(session, NULL, password);

    if (res != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Error authenticating with password: %s\n",
        ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    if (run_remote_cmd(session, cmd) != SSH_OK) {
        printf("Error executing request\n");
        ssh_get_error(session);
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    } else {
        printf("\nRequest completed successfully!\n");
    }

    ssh_disconnect(session);
    ssh_free(session);
    return 0;
}