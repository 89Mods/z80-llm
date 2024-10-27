#include <cstdint>
#include <map>

struct GenToken {
	std::string token;
	int count;
};

struct BytePair {
	std::string left;
	std::string right;
	void print() {
		std::cout << "(" << left << ") (" << right << ")" << std::endl;
	}
};

const char special_chars[] = ",.-+_ \"/()%&$!?\n\t':;0123456789\\";
class BytePairEncoder {
private:
	std::vector<BytePair*> pairs;
	std::vector<std::string> tokens;
	std::map<std::string, std::vector<std::string>> cache;
public:
	~BytePairEncoder() {
		for(int i = 0; i < pairs.size(); i++) delete pairs[i];
	}
	BytePairEncoder() {
		cache.clear();
		std::ifstream input("./data/vocab.bpe", std::ios::binary | std::ios::in);
		char rbuff[512];
		char tbuff[64];
		int ptr = 512;
		int tptr = 0;
		BytePair* curr_pair = new BytePair();
		while(1) {
			if(ptr >= 512) {
				for(int i = 0; i < 512; i++) rbuff[i] = 0;
				input.read(rbuff, 512);
				ptr = 0;
			}
			if(rbuff[ptr] == 0) break;
			tbuff[tptr] = rbuff[ptr];
			ptr++;
			if(tbuff[tptr] == ' ') {
				tbuff[tptr] = 0;
				curr_pair->left = std::string(tbuff);
				tptr = 0;
			}else if(tbuff[tptr] == '\n') {
				tbuff[tptr] = 0;
				curr_pair->right = std::string(tbuff);
				pairs.push_back(curr_pair);
				curr_pair = new BytePair();
				tptr = 0;
			}else tptr++;
		}
		input.close();
		
		std::ifstream dict_input("./data/dict.bin", std::ios::binary | std::ios::in);
		uint32_t offset;
		dict_input.read((char *)&offset, 4);
		dict_input.seekg(offset, std::ios::beg);
		uint16_t last_id = 65535;
		char strbuff[256];
		while(true) {
			uint16_t token_id;
			dict_input.read((char *)&token_id, 2);
			if(last_id + 1 != token_id && last_id != 65535) break; //Probably the end of file
			last_id = token_id;
			for(int j = 0;;j++) {
				dict_input.read(strbuff+j, 1);
				if(strbuff[j] == 0) break;
			}
			tokens.push_back(std::string(strbuff));
		}
		dict_input.close();
	}
	
	std::vector<BytePair> get_pairs(std::vector<std::string> str_parts) {
		int ptr = 0;
		std::vector<BytePair> pairs;
		for(int i = 0; i < str_parts.size() - 1; i++) {
			BytePair newPair;
			newPair.left = str_parts[i];
			newPair.right = str_parts[i + 1];
			pairs.push_back(newPair);
		}
		return pairs;
	}
	
	std::vector<std::string> bpe(const char* input) {
		std::string input_str(input);
		if(cache.find(input_str) != cache.end()) {
			return cache[input_str];
		}
		std::vector<std::string> str_parts;
		int ptr = 0;
		while(1) {
			if(input[ptr] == 0) break;
			char buff[2];
			buff[0] = input[ptr];
			buff[1] = 0;
			str_parts.push_back(std::string(buff));
			ptr++;
		}
		std::vector<BytePair> str_pairs = get_pairs(str_parts);
		if(str_pairs.size() == 0) {
			return str_parts;
		}
		int iter = 0;
		while(true) {
#ifdef PRINTD_TOKENIZER
			std::cout << "Iteration " << (iter++) << std::endl;
			for(int k = 0; k < str_parts.size(); k++) std::cout << "(" << str_parts[k] << ") ";
			std::cout << std::endl;
#endif
			BytePair* first = NULL;
			for(int i = 0; i < pairs.size(); i++) {
				for(int j = 0; j < str_pairs.size(); j++) {
					BytePair curr = str_pairs[j];
					if(pairs[i]->left == curr.left && pairs[i]->right == curr.right) {
						first = pairs[i];
						i = pairs.size() + 1;
						break;
					}
				}
			}
			if(!first) {
				//No more matches found. We’re done.
				break;
			}
			int i = 0;
			std::vector<std::string> new_str_parts;
			while(i < str_parts.size()) {
				int found = -1;
				for(int k = i; k < str_parts.size(); k++) {
					if(str_parts[k] == first->left) {
						found = k;
						break;
					}
				}
				if(found != -1) {
					for(int k = i; k < found; k++) {
						new_str_parts.push_back(str_parts[k]);
					}
					i = found;
				}else {
					for(int k = i; k < str_parts.size(); k++) {
						new_str_parts.push_back(str_parts[k]);
					}
					break;
				}
				
				if(str_parts[i] == first->left && i < str_parts.size()-1 && str_parts[i+1] == first->right) {
					std::stringstream ss;
					ss << first->left << first->right;
					new_str_parts.push_back(ss.str());
					i += 2;
				}else {
					new_str_parts.push_back(str_parts[i]);
					i++;
				}
			}
			
			str_parts = new_str_parts;
			if(str_parts.size() == 1) break;
			str_pairs = get_pairs(str_parts);
		}
#ifdef PRINTD_TOKENIZER
		std::cout << "Final string pairs to be tokenized:" << std::endl;
		for(int k = 0; k < str_parts.size(); k++) std::cout << "(" << str_parts[k] << ")" << (k == str_parts.size()-1?"":"<->");
		std::cout << std::endl;
#endif
		cache[input_str] = str_parts;
		return str_parts;
	}
	
	std::string un_tokenize(int* input, int in_len) {
		std::ifstream dict_input("./data/dict.bin", std::ios::binary | std::ios::in);
		std::stringstream ss;
		ss << "";
		char strbuff[256];
		for(int i = 0; i < in_len; i++) {
			int token = input[i] - 1;
			uint32_t addr = token * 4 + 4;
			dict_input.seekg(addr, std::ios::beg);
			dict_input.read((char *)&addr, 4);
			addr += 2;
			dict_input.seekg(addr, std::ios::beg);
			for(int j = 0;;j++) {
				dict_input.read(strbuff+j, 1);
				if(strbuff[j] == 0) break;
			}
			ss << strbuff;
		}
		dict_input.close();
		return ss.str();
	}
	
	std::string get_token_string(int token) {
		if(token == 0) return "<|blank|>";
		token--;
		if(token >= tokens.size()) return "<|error|>";
		return tokens[token];
	}
	
	std::vector<int> tokenize(const char* input) {
		//std::ifstream dict_input("../data/dict.bin", std::ios::binary | std::ios::in);
		char strbuff[256];
		//uint32_t offset;
		//dict_input.read((char *)&offset, 4);
		std::vector<std::string> words = word_split(input);
		
		std::vector<int> res;
		for(int l = 0; l < words.size(); l++) {
			std::vector<std::string> pairs = bpe(words[l].c_str());
			
			uint16_t token_id;
			int arr[pairs.size()];
			for(int i = 0; i < pairs.size(); i++) {
				uint16_t last_id = 65535;
				/*dict_input.seekg(offset, std::ios::beg);
				bool found = false;
				while(true) {
					dict_input.read((char *)&token_id, 2);
					if(last_id + 1 != token_id && last_id != 65535) break;
					last_id = token_id;
					for(int j = 0;;j++) {
						dict_input.read(strbuff+j, 1);
						if(strbuff[j] == 0) break;
					}
					if(std::string(strbuff) == pairs[i]) {
						found = true;
						break;
					}
				}*/
				for(last_id = 0; last_id < tokens.size(); last_id++) {
					if(tokens[last_id] == pairs[i]) break;
				}
				if(last_id != tokens.size()) {
					arr[i] = last_id + 1;
				}else arr[i] = 0;
			}
			for(int i = 0; i < pairs.size(); i++) res.push_back(arr[i]);
			//torch::Tensor to_merge = torch::from_blob(arr, {(long)pairs.size()}).clone();
			//res = torch::cat({res, to_merge});
		}
		
		//dict_input.close();
#ifdef PRINTD_TOKENIZER
		std::cout << "Tokenization result:" << std::endl << res << std::endl;
#endif
		return res;
	}
	
	std::vector<std::string> word_split(const char* input) {
		std::vector<std::string> tokens;
		char curr_token[512];
		int cpos = 0;
		bool end_of_word = true;
		for(int i = 0;;i++) {
			char next = input[i];
			if(next == 126) next = '-';
			if((unsigned char)next == 0xE2 && (unsigned char)input[i] == 0x80 && (unsigned char)input[i] == 0x94) { //WTF am I doing?
				next = '-';
				i += 2;
			}
			if(input[i] < 0) continue;
			const char* special_check = special_chars;
			char found = 0;
			while(*special_check && next != 1) {
				if(*special_check == next) {
					found = next;
					break;
				}
				special_check++;
			}
			if(found || next == 0) {
				if(cpos != 0) {
					if(end_of_word) {
						for(int i = cpos; i > 0; i--) curr_token[i] = curr_token[i - 1];
						curr_token[0] = 126;
						curr_token[cpos+1] = 0;
					}else curr_token[cpos] = 0;
					std::string ns = std::string(curr_token);
					tokens.push_back(ns);
				}
				end_of_word = found == ' ' || found == '\n' || found == '\t';
				if(!end_of_word && next) {
					curr_token[0] = found;
					curr_token[1] = 0;
					std::string ns = std::string(curr_token);
					tokens.push_back(ns);
				}
				cpos = 0;
			}else {
				curr_token[cpos] = next; cpos++;
			}
			if(next == 0) break;
		}
		return tokens;
	}
};
