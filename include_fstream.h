
// include_fstream.h - Mock for system header <fstream>.

class ifstream {
public:
	bool is_open();
	void open(char *);
	void close();
	void ignore();
	bool eof();
};

class ofstream {
public:
	bool is_open();
	void open(char *);
	void close();
	bool eof();
};

