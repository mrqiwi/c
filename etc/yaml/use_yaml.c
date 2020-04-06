#include <stdio.h>
#include <stdbool.h>
#include <yaml.h>

#define SIZE 64

typedef struct {
  char address[SIZE];
  char port[SIZE];
  char username[SIZE];
  char password[SIZE];
} conf_t;

int main(void)
{
    bool key;
    char *data, *tk;
    yaml_parser_t parser;
    yaml_token_t token;
    conf_t conf;

    FILE *fp = fopen("data.yaml", "r");
    if (!fp)
        printf("Failed to open file!\n");

    if (!yaml_parser_initialize(&parser))
        printf("Failed to initialize parser!\n");

    yaml_parser_set_input_file(&parser, fp);

    do {
        yaml_parser_scan(&parser, &token);
        switch(token.type) {
            case YAML_KEY_TOKEN: key = true; break;
            case YAML_VALUE_TOKEN: key = false; break;
            case YAML_SCALAR_TOKEN:
                tk = token.data.scalar.value;
                if (key) {
                    if (strcmp(tk, "address") == 0)
                        data = conf.address;
                    else if (strcmp(tk, "port") == 0)
                        data = conf.port;
                    else if (strcmp(tk, "username") == 0)
                        data = conf.username;
                    else if (strcmp(tk, "password") == 0)
                        data = conf.password;
                    else
                        printf("Unrecognised key: %s\n", tk);
                } else {
                    strncpy(data, tk, SIZE-1);
                }
                break;
            default: break;
        }
        if (token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while (token.type != YAML_STREAM_END_TOKEN);

    yaml_token_delete(&token);
    yaml_parser_delete(&parser);
    fclose(fp);

    printf("address = %s\n", conf.address);
    printf("port = %s\n", conf.port);
    printf("username = %s\n", conf.username);
    printf("password= %s\n", conf.password);

    return 0;
}
