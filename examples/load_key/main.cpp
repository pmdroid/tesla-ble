#include <client.h>
#include <utils.h>

int main() {
  TeslaBLE::Client client = TeslaBLE::Client{};

  unsigned char private_key[227] =
	  "-----BEGIN EC PRIVATE KEY-----\nMHcCAQEEIA/T6dmilMI5qenpHcHZSwAgjv4Av1aeV3xp4od09cNsoAoGCCqGSM49\nAwEHoUQDQgAE+IbvqJDY4i0sKbwgD/N1Z7rbbQRjAgt+SNwJ/PWdsC0fcph0sVvg\nd18hOwU/UxkwGY3xtrz4Vd/JpoD4Tp6+iQ==\n-----END EC PRIVATE KEY-----";
  /*
   * this loads an existing private key and generates the public key
   */
  client.LoadPrivateKey(private_key, sizeof private_key);

  unsigned char private_key_buffer[300];
  size_t private_key_length = 0;
  client.GetPrivateKey(private_key_buffer, sizeof(private_key_buffer),&private_key_length);
  TeslaBLE::DumpBuffer("Private key:\n", private_key_buffer, private_key_length);

  /*
  * set up the counter
  * this is needed because the nonce of the encrypted message is the last message count + 1
  */
  uint counter = 0;
  client.SetCounter(&counter);

  // build the whitelist message
  unsigned char message_buffer[200];
  size_t message_buffer_length = 0;
  client.BuildWhiteListMessage(message_buffer, &message_buffer_length);

  /*
   * the whitelist message can be sent to the car to whitelist the key we just generated
   */
  TeslaBLE::DumpHexBuffer("Whitelist message:\n", message_buffer, message_buffer_length);
}