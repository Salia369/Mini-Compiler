
// include_string.h - Mock for system header <string>.

#include <string.h>

class string {
public:
	string(char *);
	char *c_str();
private:
	char *data;
};

string::string(char *s)
{
	data = strdup(s);
}

char *string::c_str()
{
	return data;
}
