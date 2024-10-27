#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>

#include "tokenizer.cpp"

int main(int argc, char *argv[]) {
	if(argc < 3) {
		std::cout << "Not enough args" << std::endl;
		return 1;
	}
	BytePairEncoder bpe;
	std::string infolder = std::string(argv[1]);
	std::string outfolder = std::string(argv[2]);
	
	DIR* dir = opendir(infolder.c_str());
	while(true) {
		struct dirent* ent = readdir(dir);
		if(!ent) break;
		if(ent->d_name[0] == '.') continue; //Ignore ., .. and all hidden files
		std::stringstream ss;
		ss << infolder << "/" << ent->d_name;
		std::stringstream ss2;
		ss2 << outfolder << "/" << ent->d_name;
		std::cout << ss.str() << std::endl;
		
		std::ifstream f_in(ss.str(), std::ios::binary | std::ios::in);
		std::ofstream f_out(ss2.str(), std::ios::binary | std::ios::out);
		int total_entries = 0;
		f_in.read((char *)&total_entries, 4);
		f_out.write((char *)&total_entries, 4);
		unsigned int zero = 0;
		for(int i = 0; i < total_entries*2; i++) f_out.write((char *)&zero, 4);
		unsigned int out_pos = total_entries*8+4;
		for(int i = 0; i < total_entries; i++) {
			std::cout << (i+1) << " / " << total_entries << std::endl;
			f_in.seekg(i*8+4, std::ios::beg);
			unsigned int entry_start = 0;
			unsigned int entry_length = 0;
			f_in.read((char *)&entry_start, 4);
			f_in.read((char *)&entry_length, 4);
			f_in.seekg(entry_start, std::ios::beg);
			
			std::stringstream the_whole_line;
			char buffer[513];
			unsigned int pos = 0;
			while(1) {
				for(int i = 0; i < 513; i++) buffer[i] = 0;
				f_in.read(buffer, 512);
				pos += 512;
				the_whole_line << std::string(buffer);
				if(pos >= entry_length) break;
			}
			
			unsigned int out_pos_pre = out_pos;
			std::vector<int> a = bpe.tokenize(the_whole_line.str().c_str());
			//std::cout << bpe.un_tokenize(a.data(), a.size()) << std::endl;
			//exit(1);
			for(int j = 0; j < a.size(); j++) {
				unsigned short val = (unsigned short)a[j];
				f_out.write((char *)&val, 2);
				out_pos += 2;
			}
			unsigned int prev_pos = f_out.tellp();
			f_out.seekp(i*8+4, std::ios::beg);
			unsigned int siz = a.size() * 2;
			f_out.write((char *)&out_pos_pre, 4);
			f_out.write((char *)&siz, 4);
			f_out.seekp(prev_pos, std::ios::beg);
		}
		
		f_in.close();
		f_out.close();
	}
	closedir(dir);
	
	return 0;
}
