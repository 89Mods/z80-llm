#include <torch/torch.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <random>
#include <filesystem>

#include "tokenizer.cpp"

uint32_t b_fromfloat(float x, bool* trunc);
int write_param(torch::Tensor& t, bool flip, std::ofstream& output);

struct SelfAttentionImpl : torch::nn::Module {
public:
	SelfAttentionImpl(int d_in, int d_out, int context_length, float dropout_rate) : dropout(dropout_rate) {
		assert(d_out > 0 && d_in > 0 && context_length > 0);
		float k = sqrt(1.0f / (float)d_in);
		W_query = register_parameter("Wq", torch::rand({d_in, d_out}) * (2*k) - k);
		W_key = register_parameter("Wk", torch::rand({d_in, d_out}) * (2*k) - k);
		W_value = register_parameter("Wv", torch::rand({d_in, d_out}) * (2*k) - k);
		mask = register_buffer("mask", torch::triu(torch::ones({context_length, context_length}), 1));
		register_module("drop", dropout);
		sdk = sqrt((double)context_length);
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/keys.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W_key, false, output);
			output.close();
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/queries.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W_query, false, output);
			output.close();
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/values.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W_value, false, output);
			output.close();
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor keys = torch::matmul(input, W_key);
		torch::Tensor queries = torch::matmul(input, W_query);
		torch::Tensor values = torch::matmul(input, W_value);
		
		auto transposed = torch::transpose(keys, 1, 2);
		torch::Tensor attn_scores = torch::matmul(queries, transposed);
		torch::Tensor masked = torch::masked_fill(attn_scores, mask.to(torch::ScalarType::Bool), -std::numeric_limits<float>::infinity());
		
		torch::Tensor smax = masked / sdk;
		smax = torch::softmax(smax, -1);
		torch::Tensor attn_weights = dropout->forward(smax);
		
		torch::Tensor context_vec = torch::matmul(attn_weights, values);
		
		return context_vec;
	}
private:
	torch::Tensor W_key, W_query, W_value, mask;
	torch::nn::Dropout dropout;
	float sdk;
};
TORCH_MODULE(SelfAttention);

/*class MultiHeadAttentionImpl : torch::nn::Module {
public:
	MultiHeadAttentionImpl(int d_in, int d_out, int context_length, float dropout_rate, int num_heads) {
		assert(num_heads > 0);
		for(int i = 0; i < num_heads; i++) {
			SelfAttention head(d_in, d_out, context_length, dropout_rate);
			heads.push_back(head);
			std::stringstream ss;
			ss << "h" << i;
			register_module(ss.str(), head);
		}
		out_dim = d_out;
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		for(int i = 0; i < heads.size(); i++) {
			std::stringstream dir_name;
			dir_name << dir_base << "/" << i;
			heads[i]->save(dir_name.str());
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor result = torch::zeros({input.size(0), input.size(1), out_dim});
		for(int i = 0; i < heads.size(); i++) {
			result += heads[i]->forward(input);
		}
		return result;
	}
private:
	std::vector<SelfAttention> heads;
	int out_dim;
};
TORCH_MODULE(MultiHeadAttention);*/

struct MultiHeadAttentionAltImpl : torch::nn::Module {
public:
	MultiHeadAttentionAltImpl(int d_in, int d_out, int context_length, float dropout_rate, int num_heads) {
		assert(d_out % num_heads == 0);
		head_dim = d_out / num_heads;
		for(int i = 0; i < num_heads; i++) {
			SelfAttention head(d_in, head_dim, context_length, dropout_rate);
			heads.push_back(head);
			std::stringstream ss;
			ss << "h" << i;
			register_module(ss.str(), head);
		}
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		for(int i = 0; i < heads.size(); i++) {
			std::stringstream dir_name;
			dir_name << dir_base << "/" << i;
			heads[i]->save(dir_name.str());
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor results[heads.size()];
		for(int i = 0; i < heads.size(); i++) {
			results[i] = heads[i]->forward(input);
		}
		torch::Tensor res = torch::cat(torch::ArrayRef<torch::Tensor>(results, heads.size()), -1);
		return res;
	}
private:
	std::vector<SelfAttention> heads;
	int head_dim;
};
TORCH_MODULE(MultiHeadAttentionAlt);

struct LayerNormImpl : torch::nn::Module {
public:
	LayerNormImpl(int emb_dim) {
		assert(emb_dim > 0);
		e = emb_dim;
		scale = register_parameter("sc", torch::ones({emb_dim}));
		shift = register_parameter("sh", torch::zeros({emb_dim}));
	}
	
	void save(std::string fn) {
		std::ofstream output(fn, std::ios::binary | std::ios::out);
		write_param(scale, false, output);
		write_param(shift, false, output);
		output.close();
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor mean = input.mean(-1, true);
		torch::Tensor var = input.var(-1, false, true);
		torch::Tensor x = (input - mean) / torch::sqrt(var + eps);
		x = scale * x + shift;
		return x;
	}
private:
	float eps = 1e-5f;
	int e;
	torch::Tensor scale, shift;
};
TORCH_MODULE(LayerNorm);

struct FeedForwardImpl : torch::nn::Module {
public:
	FeedForwardImpl(int d_size) {
		float k = sqrt(1.0f / (float)d_size);
		W_in = register_parameter("Wi", torch::rand({d_size, 2*d_size}) * (2*k) - k);
		B_in = register_parameter("Bi", torch::zeros({2*d_size}));
		k = sqrt(1.0f / (float)(d_size*2));
		W_out = register_parameter("Wo", torch::rand({2*d_size, d_size}) * (2*k) - k);
		B_out = register_parameter("Bo", torch::zeros({d_size}));
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/ff0.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W_in, false, output);
			write_param(B_in, false, output);
			output.close();
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/ff1.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W_out, false, output);
			write_param(B_out, false, output);
			output.close();
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor x = torch::matmul(input, W_in);
		x += B_in;
		x = torch::gelu(x);
		x = torch::matmul(x, W_out);
		return x + B_out;
	}
private:
	torch::Tensor W_in,B_in,W_out,B_out;
};
TORCH_MODULE(FeedForward);

struct GPTConfig {
	int vocab_size;
	int context_length;
	int emb_dim;
	int n_heads;
	int n_layers;
	float drop_rate;
	
	void print() {
		std::cout << "GPTConfig {" << std::endl;
		std::cout << "\tvocab_size = " << vocab_size << std::endl;
		std::cout << "\tcontext_length = " << context_length << std::endl;
		std::cout << "\temb_dim = " << emb_dim << std::endl;
		std::cout << "\tn_heads = " << n_heads << std::endl;
		std::cout << "\tn_layers = " << n_layers << std::endl;
		std::cout << "\tdrop_rate = " << drop_rate << std::endl;
		std::cout << "}" << std::endl;
	}
};

struct TransformerBlockImpl : torch::nn::Module {
public:
	TransformerBlockImpl(GPTConfig& conf) : att(conf.emb_dim, conf.emb_dim, conf.context_length, conf.drop_rate, conf.n_heads), ff(conf.emb_dim)
	,norm1(conf.emb_dim), norm2(conf.emb_dim), drop_resid(conf.drop_rate), drop_ff(conf.drop_rate) {
		register_module("att", att);
		register_module("ff", ff);
		register_module("n1", norm1);
		register_module("n2", norm2);
		register_module("dr", drop_resid);
		register_module("dff", drop_ff);
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/norm1.bin";
			norm1->save(output_fn.str());
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/norm2.bin";
			norm2->save(output_fn.str());
		}
		{
			std::stringstream dir_name;
			dir_name << dir_base << "/attention";
			att->save(dir_name.str());
		}
		{
			std::stringstream dir_name;
			dir_name << dir_base << "/dense";
			ff->save(dir_name.str());
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor shortcut = input;
		torch::Tensor x = norm1->forward(input);
		x = att->forward(x);
		x = drop_resid->forward(x);
		x += shortcut;
		
		torch::Tensor x2 = norm2->forward(x);
		x2 = ff->forward(x2);
		x2 = drop_ff->forward(x2);
		return x2 + x;
	}
private:
	MultiHeadAttentionAlt att;
	FeedForward ff;
	LayerNorm norm1, norm2;
	torch::nn::Dropout drop_resid;
	torch::nn::Dropout drop_ff;
};
TORCH_MODULE(TransformerBlock);

struct GPTModelImpl : torch::nn::Module {
public:
	GPTModelImpl(GPTConfig& conf) : tok_emb(conf.vocab_size, conf.emb_dim), pos_emb(conf.context_length, conf.emb_dim)
	, drop_emb(conf.drop_rate), final_norm(conf.emb_dim) {
		register_module("tok", tok_emb);
		register_module("pos", pos_emb);
		register_module("drop", drop_emb);
		for(int i = 0; i < conf.n_layers; i++) {
			TransformerBlock t(conf);
			blocks.push_back(t);
			std::stringstream ss;
			ss << "trf" << i;
			register_module(ss.str(), t);
		}
		register_module("fn", final_norm);
		float k = sqrt(1.0f / (float)conf.emb_dim);
		W = register_parameter("W", torch::rand({conf.emb_dim, conf.vocab_size}) * (2*k) - k);
		sequence_length = conf.context_length;
		positions = torch::arange(conf.context_length);
		register_buffer("positions", positions);
	}
	
	void save(std::string dir_base) {
		std::filesystem::create_directory(dir_base);
		for(int i = 0; i < blocks.size(); i++) {
			std::stringstream dir_name;
			dir_name << dir_base << "/block" << i;
			blocks[i]->save(dir_name.str());
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/norm.bin";
			final_norm->save(output_fn.str());
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/token_embeddings.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			assert(tok_emb->parameters().size() == 1);
			write_param(tok_emb->parameters()[0], true, output);
			output.close();
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/positional_embeddings.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			assert(pos_emb->parameters().size() == 1);
			write_param(pos_emb->parameters()[0], true, output);
			output.close();
		}
		{
			std::stringstream output_fn;
			output_fn << dir_base << "/unembedding.bin";
			std::ofstream output(output_fn.str(), std::ios::binary | std::ios::out);
			write_param(W, false, output);
			output.close();
		}
	}
	
	torch::Tensor forward(torch::Tensor input) {
		torch::Tensor tok_embeds = tok_emb->forward(input);
		auto a = positions.to(input.device());
		torch::Tensor pos_embeds = pos_emb->forward(a);
		torch::Tensor x = tok_embeds + pos_embeds;
		x = drop_emb->forward(x);
		for(int i = 0; i < blocks.size(); i++) {
			x = blocks[i]->forward(x);
		}
		x = final_norm->forward(x);
		torch::Tensor res = torch::matmul(x, W);
		return res;
	}
private:
	std::vector<LayerNorm> norms;
	std::vector<TransformerBlock> blocks;
	torch::nn::Embedding tok_emb, pos_emb;
	torch::nn::Dropout drop_emb;
	LayerNorm final_norm;
	torch::Tensor W;
	int sequence_length;
	torch::Tensor positions;
};
TORCH_MODULE(GPTModel);

class GPTDataset {
public:
	torch::Tensor batch_inputs;
	torch::Tensor batch_targets;
	std::thread data_thread;
	GPTDataset(const char* data_folder) {
		DIR* dir = opendir(data_folder);
		while(true) {
			struct dirent* ent = readdir(dir);
			if(!ent) break;
			if(ent->d_name[0] == '.') continue; //Ignore ., .. and all hidden files
			std::stringstream ss;
			ss << data_folder << "/" << ent->d_name;
			entries.push_back(ss.str());
		}
		closedir(dir);
		std::cout << "Files in data set:" << std::endl;
		for(int i = 0; i < entries.size(); i++) std::cout << entries[i] << std::endl;
		first_time = true;
		std::random_device r;
		generator = std::default_random_engine(r());
	}
	
	void gen_batch_task(int batch_size, int context_length) {
		std::uniform_int_distribution<int> entries_dist(0, entries.size() - 1);
		std::string random_file;
		std::ifstream input;
		unsigned int num_entries;
		while(1) {
			random_file = entries.size() == 1 ? entries[0] : entries[entries_dist(generator)];
			input = std::ifstream(random_file, std::ios::in | std::ios::binary);
			num_entries = 0;
			input.read((char *)&num_entries, 4);
			if(num_entries == 0) {
				input.close();
				continue;
			}
			break;
		}
		
		torch::Tensor inputs_arr[batch_size];
		torch::Tensor targets_arr[batch_size];
		unsigned int limit = num_entries-1;
		unsigned int entry_start = 0;
		unsigned int entry_length = 0;
		for(int i = 0; i < batch_size; i++) {
			unsigned short dbuff[context_length+1];
			for(int j = 0; j < context_length+1; j++) dbuff[j] = 65535;
			std::uniform_int_distribution<int> lines_distribution(0, limit);
			unsigned int random_entry = lines_distribution(generator);
			input.seekg(random_entry*8+4, std::ios::beg);
			input.read((char *)&entry_start, 4);
			input.read((char *)&entry_length, 4);
			if(entry_start == 0 || entry_length == 0) {
				i--;
				limit = random_entry;
				continue;
			}
			if(entry_length < (context_length*2)+10) {
				i--;
				continue;
			}
			
			std::uniform_int_distribution<int> pos_distribution(0, (entry_length>>1) - context_length - 1);
			signed long random_pos = (signed long)(pos_distribution(generator));
			long seek = random_pos < 0 ? entry_start : entry_start + random_pos*2;
			input.seekg(seek, std::ios::beg);
			if(!input.read((char *)dbuff, (context_length+1) * sizeof(unsigned short))) {
				std::cout << "READ FAIL" << std::endl;
				std::cout << seek << std::endl;
				std::cout << random_pos << std::endl;
				std::cout << random_entry << std::endl;
				std::cout << num_entries << std::endl;
				std::cout << entry_start << std::endl;
				std::cout << entry_length << std::endl;
				exit(1);
			}
			int tokens_to_include = context_length+1;
			if((rand() & 31) == 0) {
				std::uniform_int_distribution<int> tokens_distribution(0, context_length - 10-1);
				tokens_to_include = tokens_distribution(generator) + 8;
			}
			long tokens_for_tensor[context_length+1];
			for(int i = 0; i < context_length+1; i++) tokens_for_tensor[i] = i < (context_length+1-tokens_to_include) ? 0 : dbuff[i];
			torch::TensorOptions opts = torch::TensorOptions().dtype(torch::kInt64);
                        inputs_arr[i] = torch::from_blob(tokens_for_tensor, {(long)context_length}, opts).clone();
                        targets_arr[i] = torch::from_blob(tokens_for_tensor+1, {(long)context_length}, opts).clone();
		}
		input.close();
		
		task_res_inputs = torch::stack(torch::ArrayRef<torch::Tensor>(inputs_arr, batch_size));
		task_res_targets = torch::stack(torch::ArrayRef<torch::Tensor>(targets_arr, batch_size));
	}
	
	void start_task(GPTConfig& cfg, int batch_size) {
		data_thread = std::thread(&GPTDataset::gen_batch_task, this, batch_size, cfg.context_length);
	}
	
	void random_batch(GPTConfig& cfg, int batch_size) {
		if(entries.size() == 0) {
			return;
		}
		if(first_time) start_task(cfg, batch_size);
		first_time = false;
		data_thread.join();
		batch_inputs = task_res_inputs.clone();
		batch_targets = task_res_targets.clone();
		start_task(cfg, batch_size);
	}
private:
	std::vector<std::string> entries;
	bool first_time;
	torch::Tensor task_res_inputs;
	torch::Tensor task_res_targets;
	std::default_random_engine generator;
};

GPTConfig default_gpt = {.vocab_size = 50257, .context_length = 512, .emb_dim = 1024, .n_heads = 16, .n_layers = 24, .drop_rate = 0.05f};
const int batch_size = 8;
const int checkpoint_interval = 128;
const float lr = 0.0001;

void benchmark_tokenizer() {
	GPTDataset training_set("/home/lucah/Documents/data");
	long start = time(NULL);
	for(int i = 0; i < 32; i++) {
		std::cout << i << std::endl;
		training_set.random_batch(default_gpt, 4);
	}
	long end = time(NULL);
	long diff = end - start;
	std::cout << diff << std::endl;
	exit(1);
}

std::string sample(GPTModel& gpt, torch::Device device, std::string input, int num_tokens_to_generate, float temperature, BytePairEncoder& enc) {
	if(input.length() > default_gpt.context_length) {
		input = input.substr(input.length() - default_gpt.context_length);
	}
	gpt->eval();
	std::stringstream ss;
	auto opts = torch::TensorOptions().dtype(torch::kInt32);
	std::vector<int> v1 = enc.tokenize(input.c_str());
	torch::Tensor b1 = torch::from_blob(v1.data(), {(long)v1.size()}, opts).to(torch::kInt64);
	b1 = torch::nn::functional::pad(b1,torch::nn::functional::PadFuncOptions({default_gpt.context_length - b1.size(0),0}).mode(torch::kConstant).value(0)).unsqueeze(0);
	b1 = b1.to(device);
	for(unsigned int i = 0; i < num_tokens_to_generate; i++) {
		torch::Tensor output = gpt->forward(b1).to(torch::kCPU)[0][-1];
		{
			torch::Tensor topk = std::get<0>(torch::topk(output, 8));
			torch::Tensor a = torch::ones({1});
			a[0] = -std::numeric_limits<float>::infinity();
			output = torch::where(output < topk[-1], a, output);
		}
		
		output /= temperature;
		output = torch::softmax(output, -1);
		int idx_next = torch::multinomial(output, 1).item<int>();
		ss << enc.get_token_string(idx_next);
		b1 = torch::roll(b1, -1);
		b1[0][default_gpt.context_length-1] = idx_next;
	}
	gpt->train();
	return ss.str();
}

void debug_print_whole(torch::Tensor t, BytePairEncoder& enc) {
	assert(t.size(0) == 512);
	for(int i = 0; i < t.size(0); i++) {
		torch::Tensor part = t[i];
		int token = torch::argmax(part).item<int>();
		std::cout << enc.get_token_string(token);
	}
	std::cout << std::endl;
}

int train_loop(torch::Device device, GPTModel gpt, unsigned short checkpoint_num);
int test_inference(torch::Device device, GPTModel gpt);

int main() {
	torch::Device device = torch::kCPU;
	if (torch::cuda::is_available()) {
		std::cout << "GPU Available, lets gooooooo" << std::endl;
		device = torch::kCUDA;
	}else {
		std::cout << "No GPU :c" << std::endl;
		//return 1;
	}
	default_gpt.print();
	
	GPTModel gpt(default_gpt);
	gpt->to(device);
	std::cout << "Counting parameters..." << std::endl;
	std::vector<torch::Tensor> params = gpt->parameters();
	unsigned long total = 0;
	for(int i = 0; i < params.size(); i++) {
		total += params[i].numel();
	}
	std::cout << total << " parameters" << std::endl;
	
	DIR* dir = opendir("checkpoints/");
	std::string newest_dir_name = "";
	long newest_time = 0;
	while(true) {
		struct dirent* ent = readdir(dir);
		if(!ent) break;
		if(ent->d_name[0] == '.') continue; //Ignore ., .. and all hidden files
		std::stringstream filename;
		filename << "checkpoints/";
		filename << ent->d_name;
		struct stat attr;
		stat(filename.str().c_str(), &attr);
		long tim = attr.st_mtime;
		if(tim > newest_time) {
			newest_time = tim;
			newest_dir_name = std::string(ent->d_name);
		}
	}
	closedir(dir);
	unsigned short checkpoint_num = 0;
	if(newest_dir_name != "") {
		std::string sub = newest_dir_name.substr(3, 2);
		checkpoint_num = std::stoi(sub);
		std::stringstream model_fn;
		model_fn << "checkpoints/gpt" << std::setfill('0') << std::setw(2) << checkpoint_num << ".pt";
		torch::load(gpt, model_fn.str(), device);
		std::cout << "Loaded " << model_fn.str() << std::endl;
	}
	
	gpt->save("model");
	{
		std::ofstream output("gelu.bin", std::ios::binary | std::ios::out);
		bool trunc;
		torch::Tensor test = torch::ones({1});
		for(int j = 0; j < 2; j++) {
			for(uint32_t i = 0; i < 0x00100000; i++) {
				float x = (float)i / 65536.0f;
				if(j) x = -x;
				uint32_t v;
				test[0] = x;
				auto res = torch::gelu(test);
				x = res.item<float>();
				std::cout << x << std::endl;
				v = b_fromfloat(x, &trunc);
				output.write((char *)&v, 4);
			}
		}
		output.close();
	}
	
	/*std::ofstream output("exp.bin", std::ios::binary | std::ios::out);
	for(int j = 0; j < 2; j++) {
		for(uint32_t i = 0; i < 0x000A0000; i++) {
			float x = (float)i / 65536.0f;
			if(j) x = -x;
			x = exp(x);
			bool trunc;
			int32_t v = b_fromfloat(x, &trunc);
			output.write((char *)&v, 4);
			if(trunc) std::cout << "Not good" << std::endl;
		}
	}
	output.close();*/
	
	//return train_loop(device, gpt, checkpoint_num);
	gpt->eval();
	BytePairEncoder tenc;
	std::string test_string = "I am a cute and soft Avali, I am a birb from outer space, lalalala, underfloofs!";
	auto opts = torch::TensorOptions().dtype(torch::kInt32);
	std::vector<int> v1 = tenc.tokenize(test_string.c_str());
	torch::Tensor b1 = torch::from_blob(v1.data(), {(long)v1.size()}, opts).to(torch::kInt64);
	b1 = torch::nn::functional::pad(b1,torch::nn::functional::PadFuncOptions({default_gpt.context_length - b1.size(0),0}).mode(torch::kConstant).value(0)).unsqueeze(0);
	torch::Tensor output = gpt->forward(b1).to(torch::kCPU)[0][-1];
	
	return 0;
}

int test_inference(torch::Device device, GPTModel gpt) {
	gpt->eval();
	BytePairEncoder tenc;
	std::string test_string = "Anna stared out the window and watched the monotonous structures of the human city quickly being replaced by an expanse of green fields. Underneath her, the old wheels of the train screeched and shuddered, vibrating her in her seat, the only one that was perfectly aligned with a window, as most of the others had been removed long ago, leaving the carriage mostly empty and its plain metal walls and floor entirely exposed. She was probably";
	float temp = 0.8f;
	std::cout << "(With temperature " << temp <<  ") [...] She was probably";
	//std::cout << "(With temperature " << temp <<  ") " << test_string;
	fflush(stdout);
	std::cout << sample(gpt, device, test_string, 128, temp, tenc);
	std::cout << std::endl;
	return 0;
}

int train_loop(torch::Device device, GPTModel gpt, unsigned short checkpoint_num) {
	BytePairEncoder tenc;
	std::cout <<  tenc.tokenize("I am a cute and soft Avali, I am a birb from outer space, lalalala, underfloofs!") << std::endl;
	gpt->train();
	
	GPTDataset training_set("../../out_lines/");
	std::cout << "{" << std::endl;
	std::cout << "\tlearning_rate = " << lr << std::endl;
	std::cout << "\tbatch_size = " << batch_size << std::endl;
	std::cout << "\tcheckpoint_interval = " << checkpoint_interval << std::endl;
	std::cout << "}" << std::endl;
	
	torch::optim::AdamW optimizer(gpt->parameters(), torch::optim::AdamWOptions(lr).weight_decay(0.02));
	optimizer.zero_grad(); gpt->zero_grad();
	std::stringstream opt_fn;
	opt_fn << "checkpoints/opt" << std::setfill('0') << std::setw(2) << checkpoint_num << ".pt";
	torch::load(optimizer, opt_fn.str(), device);
	std::cout << "Loaded " << opt_fn.str() << std::endl;
	
	int iteration = 0;
	double perplexity_sum = 0;
	
	while(1) {
		if((iteration % checkpoint_interval) == 0) {
			std::cout << "[";
			for(int i = 0; i < checkpoint_interval; i++) std::cout << " ";
			std::cout << "]\r[";
			fflush(stdout);
		}
		
		training_set.random_batch(default_gpt, batch_size);
		/*for(int i = 0; i < 2; i++) {
			torch::Tensor data = i == 0 ? training_set.batch_inputs[0] : training_set.batch_targets[0];
			for(int j = 0; j < 512; j++) std::cout << tenc.get_token_string(data[j].item<long>());
			std::cout << std::endl << std::endl;
		}*/

		torch::Tensor out = gpt->forward(training_set.batch_inputs.to(device));
		//for(int i = 0; i < batch_size; i++) {
		//	debug_print_whole(out[0], tenc);
		//	std::cout << std::endl;
		//}
		//exit(1);
		
		torch::Tensor targs = training_set.batch_targets.flatten(0, 1).to(device);
		torch::Tensor loss = torch::nn::functional::cross_entropy(out.flatten(0, 1), targs);
		perplexity_sum += exp(loss.item<float>());
		loss.backward();
		
		iteration++;
		std::cout << ".";
		fflush(stdout);
		
		if((iteration % checkpoint_interval) == 0) {
			optimizer.step();
			optimizer.zero_grad(); gpt->zero_grad();
			
			checkpoint_num &= 3;
			std::cout << "]" << std::endl << "Checkpoint reached [checkpoints/gpt" << std::setfill('0') << std::setw(2) << checkpoint_num << ".pt]" << std::endl;
			std::cout << "Mean perplexity at checkpoint: " << (perplexity_sum / (double)checkpoint_interval) << std::endl;
			perplexity_sum = 0;
			
			std::stringstream filename;
			filename << "checkpoints/gpt";
			filename << std::setfill('0') << std::setw(2) << checkpoint_num;
			filename << ".pt";
			torch::save(gpt, filename.str());
			std::stringstream opt_filename;
			opt_filename << "checkpoints/opt";
			opt_filename << std::setfill('0') << std::setw(2) << checkpoint_num;
			opt_filename << ".pt";
			torch::save(optimizer, opt_filename.str());
			checkpoint_num++;
			
			{
				std::string test_string = "Anna stared out the window and watched the monotonous structures of the human city quickly being replaced by an expanse of green fields. Underneath her, the old wheels of the train screeched and shuddered, vibrating her in her seat, the only one that was perfectly aligned with a window, as most of the others had been removed long ago, leaving the carriage mostly empty and its plain metal walls and floor entirely exposed. She was probably";
				float temp = (float)(rand() & 255) / 255.0f + 0.5f;
				std::cout << "(With temperature " << temp <<  ") [...] She was probably";
				fflush(stdout);
				std::cout << sample(gpt, device, test_string, 24, temp, tenc);
				std::cout << std::endl;
			}
		}
	}
	
	return 0;
}

uint32_t b_fromfloat(float x, bool* trunc) {
	*trunc = false;
	bool neg = false;
	if(x < 0) {
		neg = true;
		x = -x;
	}
	x *= 65536.0f;
	if(x > 0x7FFFFFFF) {
		*trunc = true;
		return neg ? 0x80000000 : 0x7FFFFFFF;
	}
	uint32_t res = (uint32_t)x;
	if(res == 0 && x != 0) *trunc = true;
	if(neg) {
		res = ~res;
		res++;
	}
	return res;
}

int write_param(torch::Tensor& t, bool flip, std::ofstream& output) {
	int trunc_count = 0;
	auto sizes = t.sizes();
	if(sizes.size() == 1 || sizes[1] == 0) {
		int width = sizes[0];
		std::cout << "Saving vector of length " << width << std::endl;
		//std::cout << t << std::endl;
		for(int x1 = 0; x1 < width; x1++) {
			float val = t[x1].item<float>();
			//bool trunc;
			//uint32_t quantized_short = b_fromfloat(val, &trunc);
			//output.write((char *)&quantized_short, 4);
			//if(trunc) trunc_count++;
			output.write((char *)&val, 4);
		}
	}else {
		int width = sizes[0];
		int height = sizes[1];
		std::cout << "Saving matrix of size " << width << "x" << height << std::endl;
		//std::cout << t << std::endl;
		if(flip) {
			for(int y1 = 0; y1 < width; y1++) {
				for(int x1 = 0; x1 < height; x1++) {
					float val = t[y1][x1].item<float>();
					//bool trunc;
					//uint32_t quantized_short = b_fromfloat(val, &trunc);
					//if(trunc) trunc_count++;
					//output.write((char *)&quantized_short, 4);
					output.write((char *)&val, 4);
				}
			}
		}else {
			for(int x1 = 0; x1 < height; x1++) {
				for(int y1 = 0; y1 < width; y1++) {
					float val = t[y1][x1].item<float>();
					//bool trunc;
					//uint32_t quantized_short = b_fromfloat(val, &trunc);
					//if(trunc) trunc_count++;
					//output.write((char *)&quantized_short, 4);
					output.write((char *)&val, 4);
				}
			}
		}
	}
	return trunc_count;
}
