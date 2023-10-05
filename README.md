# CPPTeslaBLE
This CPP library facilitates direct communication with Tesla vehicles via the BLE API. It offers fundamental features such as unlocking/locking, opening the trunk, and more. The library's capabilities are contingent on the range of actions implemented by Tesla, which is the only limitation at present.

## Rationale behind the Library
In my current residence, the Tesla vehicle lacks access to the mobile network but is connected to Wi-Fi. I posited that the car would establish a Wi-Fi connection when it's active. While the conventional wake-up method relies on the mobile network, this option isn't viable in this scenario. Thus, I explored the possibility of using Bluetooth for this purpose, and it proved successful.

## Usage
Several examples are included for your convenience.
```bash
cd examples/simple/ 
mkdir -p build/
cd build/
cmake ..
make
```

# IMPORTANT
Please take note that this library does not have official backing from Tesla, and its operational capabilities may be discontinued without prior notice. It's essential to recognize that this library retains private keys and other sensitive data on your device without encryption. I would like to stress that I assume no liability for any possible (although highly unlikely) harm that may befall your vehicle.