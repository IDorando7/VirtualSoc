#pragma once

void Parser(char buffer[], char **cmd, char **arg1, char **arg2);
int read_and_normalize(char buffer[], int size);
void trim_newline(char *s);