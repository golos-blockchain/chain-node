# Smart Exchange Tests

### Hybrid is better than multi; multi is better than direct; fix sell (0.400 GBG -> 0.250)

```sh
create_order cyberfounder 1 "6.000 GOLOS" "0.100 GBG" false 0 true
create_order cyberfounder 2 "10.000 GOLOS" "1.000 GBG" false 0 true
create_order cyberfounder 3 "50.000 GOLOS" "10.000 GBG" false 0 true

create_asset cyberfounder "100000.000 AAA" true false "{}" true
issue_asset cyberfounder "5.000 AAA" cyberfounder true

create_order cyberfounder 4 "11.000 GOLOS" "0.002 AAA" false 0 true
create_order cyberfounder 5 "0.002 AAA" "0.500 GBG" false 0 true
```

### Same but slightly another

```
create_order cyberfounder 1 "6.000 GOLOS" "0.100 GBG" false 0 true
create_order cyberfounder 2 "10.000 GOLOS" "1.000 GBG" false 0 true
create_order cyberfounder 3 "50.000 GOLOS" "10.000 GBG" false 0 true

create_asset cyberfounder "100000.000 AAA" true false "{}" true
issue_asset cyberfounder "5.000 AAA" cyberfounder true

create_order cyberfounder 4 "11.000 GOLOS" "5.000 AAA" false 0 true
create_order cyberfounder 5 "5.000 AAA" "0.500 GBG" false 0 true
```

// or multi=direct:

```
create_order cyberfounder 4 "10.000 GOLOS" "5.000 AAA" false 0 true
create_order cyberfounder 5 "5.000 AAA" "0.500 GBG" false 0 true
```

// or multi > direct, but not enough:

```
create_order cyberfounder 4 "10.000 GOLOS" "2.000 AAA" false 0 true
create_order cyberfounder 5 "2.000 AAA" "0.200 GBG" false 0 true
```
