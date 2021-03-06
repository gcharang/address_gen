#include <iostream>
#include <fstream>
#include <cstdint>
#include <string> 
#include <bitcoin/system.hpp>

#include <chrono>
#include <pthread.h>

/* Vanity Multisig Example by Decker (q) 2019 */

using namespace std;
using namespace libbitcoin::system;
using namespace libbitcoin::system::chain;
using namespace libbitcoin::system::wallet;

#define MAX_THREADS 2
static const std::string start_pattern = "start";
static const std::string end_pattern = "end";
static const std::string find_pattern = "decker";
pthread_mutex_t my_lock;

/* 
    https://github.com/libbitcoin/libbitcoin-system/wiki/Examples-from-Serialised-Data
    https://stackoverflow.com/questions/505021/get-bytes-from-stdstring-in-c
    http://calaganne.blogspot.com/2017/04/libbitcoin-bx-seed.html
    
*/

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0)
{
	// Convert complete given String to lower case
	std::transform(data.begin(), data.end(), data.begin(), ::tolower);
	// Convert complete given Sub String to lower case
	std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
	// Find sub string in given string
	return data.find(toSearch, pos);
}

void* _check_passphrase(void* rawArg) {
    
    unsigned int thr_idx = *((unsigned int *) rawArg);

    /* No other thread is going to join() this one */
    // pthread_detach(pthread_self());

    std::string passphrase;

    uint64_t i;
    hash_digest hash;

    ec_secret privkey;
    ec_compressed pubkey;
    one_byte addr_prefix;
    data_chunk prefix_pubkey_checksum;

    auto start = chrono::steady_clock::now();    
    
    for (i=0; i<0x7FFFFFFF; i++) {
        
        // pattern for passphrase creation
        // passphrase = start_pattern + std::to_string(i) + end_pattern;

        // uncomment this if you want random passphrase
        
        data_chunk my_entropy(32);
        pseudo_random_fill(my_entropy);
        wallet::word_list mnemonic_words = wallet::create_mnemonic(my_entropy);
        passphrase = join(mnemonic_words); 
        
        if ((i % 1000000) == 0) {

            pthread_mutex_lock(&my_lock);
            auto end = chrono::steady_clock::now();
            cout << "thd." << setw(3) << setfill('0') << thr_idx << " [" << setw(10) << setfill('0') << i << "] " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;
            start = chrono::steady_clock::now();
            pthread_mutex_unlock(&my_lock);

        }

        std::vector<char> passphrase_bytes(passphrase.begin(), passphrase.end());
        // passphrase_bytes.push_back('\0');
        auto passphrase_data_slice = data_slice((const uint8_t *)passphrase_bytes.data(),(const uint8_t *)(passphrase_bytes.data() + passphrase_bytes.size()));
        auto sha256sum = sha256_hash(passphrase_data_slice);
        sha256sum[0]  = sha256sum[0] & 248;
        sha256sum[31] = sha256sum[31] & 127;
        sha256sum[31] = sha256sum[31] | 64;
        
        /*
        auto sha256sum_hex = encode_base16(sha256sum);
        decode_base16(privkey, sha256sum_hex);
        */
        
        privkey = sha256sum;
        secret_to_public(pubkey, privkey);

        // Pubkeyhash: sha256 + hash160
        auto my_pubkeyhash = bitcoin_short_hash(pubkey);
        
        addr_prefix = { { 60 } };
        // Byte sequence = prefix + pubkey + checksum(4-bytes)
        prefix_pubkey_checksum = to_chunk(addr_prefix);
        extend_data(prefix_pubkey_checksum, my_pubkeyhash);
        append_checksum(prefix_pubkey_checksum);

        // http://aaronjaramillo.org/libbitcoin-create-a-non-native-segwit-multisig-addressp2sh-p2wsh
        // https://github.com/AaronJaramillo/LibbitcoinTutorial/

        // creating multisig
        data_chunk pubkey1 = to_chunk(pubkey);
        data_stack keys { pubkey1 };
        script multiSig = script(script().to_pay_multisig_pattern(1, keys));;
        std::string multisig_str = payment_address(multiSig, 85).encoded();

        //cout << "KMD: " << kmd_addr << endl << "Passphrase: '" << passphrase << "'" << endl;
        size_t pos = findCaseInsensitive(multisig_str, find_pattern);
        if( pos != std::string::npos) {
            
            std::string kmd_addr = encode_base58(prefix_pubkey_checksum);
                        
            // you can directly generate Bitcoin addresses with Libbitcoin wallet types: ec_private/ec_public, instead of manual,
            // like `wallet::ec_private privateKey(rawprivateKey, 0x8000, false);` - http://aaronjaramillo.org/libbitcoin-first-program .

            one_byte secret_prefix = { { 188 } };
            one_byte secret_compressed = { { 0x01 } }; // omitted if uncompressed
            // Apply prefix, suffix & append checksum
            auto prefix_secret_comp_checksum = to_chunk(secret_prefix);
            extend_data(prefix_secret_comp_checksum, privkey);
            extend_data(prefix_secret_comp_checksum, secret_compressed);
            append_checksum(prefix_secret_comp_checksum);

            std::string kmd_wif = encode_base58(prefix_secret_comp_checksum);
            pthread_mutex_lock(&my_lock);
            cout << '\a' << flush; // beep/bell if found
            cout << endl;
            cout << "Passphrase         : '" << passphrase << "'" << endl;
            cout << "KMD address        : " << kmd_addr << endl;
            cout << "Privkey (wif)      : " << kmd_wif << endl;
            cout << "Pubkey             : " << encode_base16(pubkey) << endl;
            //cout << "Script address: " << multisig_str << endl;
            cout << "Script address     : " << multisig_str.substr(0,pos) << "\x1B[33m" << multisig_str.substr(pos, find_pattern.size()) << "\033[0m" << multisig_str.substr(pos + find_pattern.size()) << endl;
            cout << "Reedem script (asm): " << multiSig.to_string(1) << endl;
            cout << "Reedem script (hex): " << encode_base16(multiSig.to_data(false)) << endl;
            cout << endl;

            std::ofstream logfile("vanity-multisig.log", ios::out | ios::app);
            if (logfile.is_open())
            {

                logfile << endl;
                logfile << "Passphrase         : '" << passphrase << "'" << endl;
                logfile << "KMD address        : " << kmd_addr << endl;
                logfile << "Privkey (wif)      : " << kmd_wif << endl;
                logfile << "Pubkey             : " << encode_base16(pubkey) << endl;
                logfile << "Script address     : " << multisig_str << endl;
                logfile << "Reedem script (asm): " << multiSig.to_string(1) << endl;
                logfile << "Reedem script (hex): " << encode_base16(multiSig.to_data(false)) << endl;
                logfile << endl;

                logfile.close();
            }

            pthread_mutex_unlock(&my_lock);
            //cout << "KMD: " << multisig_str.substr(0,pos) << "\x1B[33m" << multisig_str.substr(pos, find_pattern.size()) << "\033[0m" << multisig_str.substr(pos + find_pattern.size()) << "\tPassphrase: '" << passphrase << "'" << endl;
        }
        
        /*
        int len = 3;
        if (btc_addr.substr(1,len) == kmd_addr.substr(1,len)) {
            cout << "KMD: " << kmd_addr << ", BTC: " << btc_addr << "\tPassphrase: '" << passphrase << "'" << endl;
        }
        */
    }

    pthread_exit(nullptr);
}

int main() 
{

    // https://eax.me/pthreads/

    pthread_t thr[MAX_THREADS];
    unsigned int ints[MAX_THREADS];

    int index = 0;

    while (index < MAX_THREADS) {
        ints[index] = index;
        if (pthread_create(&(thr[index]), nullptr, _check_passphrase, &ints[index]))
            throw std::runtime_error("pthread_create() failed");
        index++;
    }

    /* we don't need to join all threads and wait their finish,
       program will be exited when first thread will finished. */

    pthread_join(thr[0], NULL);

    std::cout << "Done!" << std::endl;

    return 0; 
}