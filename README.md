# PIN-PAM

PIN-PAM is PAM authentication module uses 4-digits PIN code.
Could be used for 2FA for PAM, e.g. sudo with face-recognition + 4-digits PIN code.

## License

This project is licensed under the MIT License - see the [LICENSE](./LICENSE) file for details.

## USAGE

Install pam module and user edit tool.

### VOID LINUX

Add repository:
```
repository=https://void.g4s8.wtf/repo
```
Install pin-pam:
```
$ sudo xbps-install -S pin-pam
```

### FROM SOURCE

 1. Clone repository
 2. Build with `make`
 3. Install with `make install`

---

Edit `/etc/pam.d/sudo`, add at the beginning (before other modules):
```
auth		sufficient	pam_pin.so
```

Example sudo file:
```
auth		sufficient	pam_pin.so

auth 		include 	system-auth
account 	include 	system-auth
session 	include 	system-auth
```

---

Add user to the list:
```
$ ppedid add g4s8
```

On the next `sudo` request pin code will be asked instead of password.
