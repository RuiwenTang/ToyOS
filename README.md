
## ToyOS

A hobby os made for fun.

### Requirement

* Ninja
* Meson
* Nasm
* Cross GCC Compile
    ```
    ./make_toolchain.sh
    ```
* QEMU or bochs

### Compile and Run

```
meson setup build --cross-file toy-os-crossfile.txt

python run_qemu.py
# or
python run_bochs.py
```
