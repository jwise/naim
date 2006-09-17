#!/bin/sh
sed 's/\\/\\\\/g
     s/"/\\"/g
     ' |
awk 'BEGIN      {printf("static const char *default_lua = \n");
                }
     /^.*$/ 	{printf("\t\"%s\\n\"\n", $0);
                }
     END        {printf(";\n");
                } '
