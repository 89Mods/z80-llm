rm -f encoder.json vocab.bpe
wget https://openaipublic.blob.core.windows.net/gpt-2/models/124M/encoder.json
node convert_dict.js
wget wget https://openaipublic.blob.core.windows.net/gpt-2/models/124M/vocab.bpe
sed -ie 's/~/-/g' vocab.bpe
sed -ie 's/Ġ/~/g' vocab.bpe
sed -i '49994d' vocab.bpe
sed -i '1d' vocab.bpe
sed -i '49272d' vocab.bpe
sed -i '35241d' vocab.bpe
sed -i '49586d' vocab.bpe
sed -i '49911d' vocab.bpe
sed -i '49923d' vocab.bpe
