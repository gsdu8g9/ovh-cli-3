## Disclaimer

* **Never ever** install this tool on servers (dedicated as VPS) you rent to OVH
* I decline all responsabilities of the usage made of this tool, there is no warranty
* I'm not affiliated in any way to the OVH company

## Prerequisites

### Build dependencies

* cmake
* (a C compiler)

### Run dependencies

* openssl
* libcurl
* libxml2
* libiconv (in the future)

## Install

```
git clone ...
cmake . \
    -DAPPLICATION_KEY="<your application key>" \
    -DAPPLICATION_SECRET="<your application secret>" \
    -DAPI_BASE_URL="https://eu.api.ovh.com/1.0"
make
```

## Commands

* account
    * add \<nickhandle> \<password or use empty string - "" - to not record it> (\<consumer key> expires in|at \<expiration date or delay>)
    * list
    * delete \<nickhandle>
    * default \<nickhandle>
    * switch \<nickhandle>
* domain
    * list
* help
* quit

NOTE (TODO): ~/.ovh will be world readable (chmod 0600 ~/.ovh)
