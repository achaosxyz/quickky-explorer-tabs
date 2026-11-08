# Windows 11 Native ABI Adaptation

## Scope and status

This document owns native `ExplorerFrame.dll` compatibility detection.
[Explorer adapters](EXPLORER.md) owns tab operations and
[Hook integration](HOOK.md) owns attachment. Windows 10 does not use these rules.
The application uses compiled-in maps only: no symbol downloads, PDB loading,
symbol cache, or background resolver. Exact public PDBs are development evidence,
not an application dependency.

The catalog covers the exact x64 images below, inspected with matching public
PDBs. Each map supplies all thirteen typed entries and full-entry fingerprints:

| Component version | CodeView GUID (age 1) | Accessor rule | Validation boundary | Image source | DLL SHA-256 | PDB locator |
| --- | --- | --- | --- | --- | --- | --- |
| `10.0.26100.9549` | `822A8C93-960A-C7B6-7F6A-87CF48E8CE6F` | 1 | Scoped live evidence and offline checks | [Installed baseline](#source-provenance) | `b7bda6011c0c13a81a6ea9670789c3ecb241caf994d31e274add18704929ca6d` | [PDB (identity-derived)](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/822A8C93960AC7B67F6A87CF48E8CE6F1/ExplorerFrame.pdb) |
| `10.0.26100.9818` | `468742F3-D41C-3441-A502-2FBFFEF9A656` | 1 | Exact-image/PDB and offline checks; no live acceptance yet | [DLL (reused download)](https://msdl.microsoft.com/download/symbols/explorerframe.dll/0127D437327000/explorerframe.dll) | `0c7f4d6623fed919e6f9addfc5d1519a7dd3716a96b833b0f26fd6e6c39ae563` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/468742F3D41C3441A5022FBFFEF9A6561/ExplorerFrame.pdb) |
| `10.0.26100.1` | `3F594208-71FE-A9BE-3371-575498A24FC7` | 2 | Exact-image/PDB and offline checks; no live acceptance yet | [DLL (reused download)](https://msdl.microsoft.com/download/symbols/explorerframe.dll/3691089927d000/explorerframe.dll) | `22689659425c7cb765a2d908e84a075482d4ca8abc0e9261c2d6b07bf71d89ad` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/3F59420871FEA9BE3371575498A24FC71/ExplorerFrame.pdb) |
| `10.0.28000.3086` | `7B7F6EF1-F61E-1768-4DBD-B199F7981957` | 3 | Exact-image/PDB; no live acceptance yet | [DLL (reused download)](https://msdl.microsoft.com/download/symbols/explorerframe.dll/1D9198F030f000/explorerframe.dll) | `762c4b9762804ef87e1dbe50214956c5ced41221d82d5975733f592d4a5f7ba2` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/7B7F6EF1F61E17684DBDB199F79819571/ExplorerFrame.pdb) |
| `10.0.28000.2605` | `46334190-881B-8CCE-8CBB-A6D18E3809CD` | 4 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/2B4B73BB303000/explorerframe.dll) | `de2c2ebff59724cb3f6c84c00eb88e3d051d437e3c5e57ecbc46046039df00ff` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/46334190881B8CCE8CBBA6D18E3809CD1/ExplorerFrame.pdb) |
| `10.0.28000.2525` | `FF6575FC-8454-293D-682E-0CF7B3DFBB6A` | 4 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/6DFCED532EF000/explorerframe.dll) | `61f630c222e2acceff3bf16cd8cb82d6e7117d8b0c80b9c154c21a1b285f5a8f` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/FF6575FC8454293D682E0CF7B3DFBB6A1/ExplorerFrame.pdb) |
| `10.0.28000.1761` | `433C17BE-CFB4-C1B3-19C2-626768F06589` | 4 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/94041AF32D3000/explorerframe.dll) | `36a618c53dd94507839a2ab334d03b70ad9700950a287d9d0928cf47d61db170` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/433C17BECFB4C1B319C2626768F065891/ExplorerFrame.pdb) |
| `10.0.26100.9278` | `E4B44D51-365E-4FA3-AA9B-BAD5CA3BB93A` | 1 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/4E726DC4318000/explorerframe.dll) | `d19f66b2e85e1c444adf3f663d5b342408db0d2afa4ea614cdccb3359f15778a` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/E4B44D51365E4FA3AA9BBAD5CA3BB93A1/ExplorerFrame.pdb) |
| `10.0.26100.8972` | `F8DF25B5-96A8-B9B9-8314-A107AAADB0B3` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/07DB0869311000/explorerframe.dll) | `35a7f3923dfe5e81630102c01420c7386ccc1080a5175efc1aa0b12191c97c79` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/F8DF25B596A8B9B98314A107AAADB0B31/ExplorerFrame.pdb) |
| `10.0.26100.8737` | `25F6494D-2336-5391-5E3B-C33ED41C9C3B` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/E30A4FB1301000/explorerframe.dll) | `e3739ec0513694c4583a4f950dac80a43d5f92807fcb2fbafd12389434ef06f4` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/25F6494D233653915E3BC33ED41C9C3B1/ExplorerFrame.pdb) |
| `10.0.26100.8655` | `02B1900B-20AF-6948-BE5E-5C0EC76FCCAC` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/8B791C5F2F3000/explorerframe.dll) | `60a2ab6b21312efaf9e4fba3611f5fab255aed68a5450d11ccdd4305a81161f3` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/02B1900B20AF6948BE5E5C0EC76FCCAC1/ExplorerFrame.pdb) |
| `10.0.22621.5413` | `A4E52999-241E-E1DB-266F-228962F81BC2` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/28ADC6792C4000/explorerframe.dll) | `db0a0ff182664086d36389be3f32f7065a0543fd7a0e9d03d79d83ddb02952c3` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/A4E52999241EE1DB266F228962F81BC21/ExplorerFrame.pdb) |
| `10.0.22621.5331` | `DB6F325F-FA06-4F1F-A770-CD1A20D1C6EF` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/6A09E43C2C4000/explorerframe.dll) | `07c8d66431bcb7886520d2b0b1fed2f35045f917fb156a0fb852f8677fc2c12c` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/DB6F325FFA064F1FA770CD1A20D1C6EF1/ExplorerFrame.pdb) |
| `10.0.22621.5262` | `36FCC227-5BB4-12D5-9C6D-4455460E9EEB` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/3730D3AD2C4000/explorerframe.dll) | `fd294e3346b77a8612422a25e749220abd22e8bd40eabfb41017a098cac9f24f` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/36FCC2275BB412D59C6D4455460E9EEB1/ExplorerFrame.pdb) |
| `10.0.22621.3958` | `B03A2527-BB6D-FC57-1C50-CC6538607749` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/08508E122B1000/explorerframe.dll) | `ef4dd681a95701f50cb5bc0b087909d3f8b3699080b4d639802de77ace264012` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/B03A2527BB6DFC571C50CC65386077491/ExplorerFrame.pdb) |
| `10.0.22621.4541` | `C07E8BBF-5F04-A49B-C02A-A07CA755B9A7` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/3D1CA2332B8000/explorerframe.dll) | `6a4a6191b11211ff4bf1ac59d922d1bc85e3974a34eea2228ce403ae737fde95` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/C07E8BBF5F04A49BC02AA07CA755B9A71/ExplorerFrame.pdb) |
| `10.0.22621.4601` | `074624E6-B9F6-E0F1-4F92-AF0FB3896551` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/43510D942B8000/explorerframe.dll) | `a6bc3ab2b433efb774c70a19847cb797b6934bf7e7685f41e567b57c35c80e2e` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/074624E6B9F6E0F14F92AF0FB38965511/ExplorerFrame.pdb) |
| `10.0.22621.4746` | `2819EF18-7F81-DA5A-610E-79AF352B87EC` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/67ECD7FA2BB000/explorerframe.dll) | `19bb6664679d83c4c063ae2ec156262b51444829eba0b06f71de86fd7b64e596` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/2819EF187F81DA5A610E79AF352B87EC1/ExplorerFrame.pdb) |
| `10.0.26100.7309` | `A7E04BA1-6DB3-EA3E-7043-D341B60F4F82` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/D636292F2BD000/explorerframe.dll) | `59099d3a41dccb1262440495f52c013f5efafcc0798a79e21e68c7cc828147c8` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/A7E04BA16DB3EA3E7043D341B60F4F821/ExplorerFrame.pdb) |
| `10.0.26100.7462` | `A13F5397-7AE4-71D4-781B-9F6BCA709529` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/59B5BDF92BD000/explorerframe.dll) | `78d1beca1a528872832b3f039aee7daa1ba9b79c98754c248794cce192f4a667` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/A13F53977AE471D4781B9F6BCA7095291/ExplorerFrame.pdb) |
| `10.0.26100.7705` | `5E537B80-9194-8BD8-B43A-056AB12076DD` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/7490D5CB2D3000/explorerframe.dll) | `d827e9b5c62c14041dd195ddb2f1185596722548195f07d00d7222d9e4739817` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/5E537B8091948BD8B43A056AB12076DD1/ExplorerFrame.pdb) |
| `10.0.26100.7824` | `11C1E947-47AD-6FFD-F761-97572130AF1C` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/B43454F32D3000/explorerframe.dll) | `b32b3c9f0f6ea82a3e7701a99113d2625bbc35d70b9af2d5538d02840280ddb1` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/11C1E94747AD6FFDF76197572130AF1C1/ExplorerFrame.pdb) |
| `10.0.26100.7920` | `F66372C6-49A7-230C-8B9A-EF22A0EDEC7E` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/7BC45DEC2D8000/explorerframe.dll) | `f040fc5067d81b27bd9a50800e92e05f2c6e39eeaa59765b37bd1e6aa157f1c4` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/F66372C649A7230C8B9AEF22A0EDEC7E1/ExplorerFrame.pdb) |
| `10.0.26100.8115` | `FAEC0B07-5559-383C-8212-CAC3401CAD92` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/3AF37BB02E2000/explorerframe.dll) | `0b57fbc38e9557d8bfcf9d12dfaca944dfd9b10b72a10379c3ebfa2743505514` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/FAEC0B075559383C8212CAC3401CAD921/ExplorerFrame.pdb) |
| `10.0.26100.8328` | `216177BC-DEB0-3937-01AF-EDAE6EBFD9FD` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/B2AFA62B2E6000/explorerframe.dll) | `55139bd1c6f7fdc6a463ec5ae161b327f159dbb336289d70a288fc7901dff87c` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/216177BCDEB0393701AFEDAE6EBFD9FD1/ExplorerFrame.pdb) |
| `10.0.26100.8457` | `E183F37E-71AB-7E36-957E-E0D4E284FC46` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/BF57B82F2E6000/explorerframe.dll) | `4e28d2076608df5525ec1b7b2ed8dc0e866f43bf2ded6a5e4213ccd1c2284cbe` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/E183F37E71AB7E36957EE0D4E284FC461/ExplorerFrame.pdb) |
| `10.0.26100.8521` | `A610550A-3A1D-79D6-B896-A3F72E55B3B2` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/55237DE52F3000/explorerframe.dll) | `fb0987c1ed1fc19b67323146a560b919824f7356f18e8a57a3826fd993ee87ec` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/A610550A3A1D79D6B896A3F72E55B3B21/ExplorerFrame.pdb) |
| `10.0.22621.3672` | `185040F1-2C18-D327-8B3C-FDE3174F020F` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/B62EC68D2A4000/explorerframe.dll) | `983c2b709e729a3e3e664b6636552d4c3b2e5baa69df2f480323f469eb68d932` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/185040F12C18D3278B3CFDE3174F020F1/ExplorerFrame.pdb) |
| `10.0.26100.5074` | `37BCC217-328C-E5F2-CDF1-3FD9CDEBF286` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/9B2F5B302BE000/explorerframe.dll) | `5b0ac84dc4eaf1ef496fd3918a5059d80a9209ce7bcfa9eee505ab18f64999ce` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/37BCC217328CE5F2CDF13FD9CDEBF2861/ExplorerFrame.pdb) |
| `10.0.26100.6725` | `8CCA1CF2-38BF-0C29-ADD1-8BAE72943DC6` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/9CA458A02C0000/explorerframe.dll) | `375b1d71b82269b3b58870a5bb03aed7e2397c140918b308026942175de24c82` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/8CCA1CF238BF0C29ADD18BAE72943DC61/ExplorerFrame.pdb) |
| `10.0.26100.7019` | `D79124F2-7E3B-B690-9994-67B1EBA077C8` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/A21FF0772BD000/explorerframe.dll) | `3f6ff4c8111c96539816999708e3426464ce36abead01d229771868be1dfba1e` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/D79124F27E3BB690999467B1EBA077C81/ExplorerFrame.pdb) |
| `10.0.26100.7171` | `5BE6EC1F-C1CF-AC7E-0FFD-515440221BB0` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/CC4E225E2BC000/explorerframe.dll) | `ec898421adc8a161947b915c0f37051dcf0b71b35e3c4c963bd6ab79daeafd5e` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/5BE6EC1FC1CFAC7E0FFD515440221BB01/ExplorerFrame.pdb) |
| `10.0.22621.5909` | `4203F893-8472-64F2-8852-84745243E287` | 5 | Exact returned image/PDB; no live acceptance yet | [Request 10.0.22621.5840 (index-derived)](https://msdl.microsoft.com/download/symbols/explorerframe.dll/C13822332C4000/explorerframe.dll) | `f41efc1012f325a956abaa6ba4e525e127f869f47f27e84380b2fb0ee8b2600c` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/4203F893847264F2885284745243E2871/ExplorerFrame.pdb) |
| `10.0.22621.6197` | `608B122F-1A03-6CED-EEE8-74D51C41129C` | 5 | Exact returned image/PDB; no live acceptance yet | [Request 10.0.22621.6133 (index-derived)](https://msdl.microsoft.com/download/symbols/explorerframe.dll/63001A8A2C0000/explorerframe.dll) | `326567898a046303753e8a4ce6be4ff736e94498731f89341fea51bd2c381c5a` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/608B122F1A036CEDEEE874D51C41129C1/ExplorerFrame.pdb) |
| `10.0.26100.4202` | `20906462-08A7-1F46-2362-BC04AD41C0A8` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/3A08991B2B9000/explorerframe.dll) | `69286f0414b6ca5a9bb2c2a2269dadd70b41309577fd68c1e3d9b2ab336e56d6` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/2090646208A71F462362BC04AD41C0A81/ExplorerFrame.pdb) |
| `10.0.26100.4768` | `52C60A00-9553-E194-8EED-0785A24A111C` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/98E3DAD32C2000/explorerframe.dll) | `de4447db6cc3f816a4a86e384cf6f360083701ce14cba1374c3e818f45bd1a6c` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/52C60A009553E1948EED0785A24A111C1/ExplorerFrame.pdb) |
| `10.0.26100.4484` | `0A714E7F-550C-BABA-0855-5DC4EC85B646` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/71C2145F2C2000/explorerframe.dll) | `1cf06ee63de63379b8b8da2400d7175a12a82de7ecde51eec915a129c997bdfe` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/0A714E7F550CBABA08555DC4EC85B6461/ExplorerFrame.pdb) |
| `10.0.26100.2454` | `6CB1D2F3-D0A3-1FC1-E1DA-DB5CE35E153B` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/5E91757228C000/explorerframe.dll) | `4dbc275b7ee6279c47fbb9862e8aa04a031aa58cf76512a004d621eb36bb2caa` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/6CB1D2F3D0A31FC1E1DADB5CE35E153B1/ExplorerFrame.pdb) |
| `10.0.26100.2605` | `8FACD8F0-1357-7EFB-B125-47A2F87A551E` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/402F162028C000/explorerframe.dll) | `949ddaa7ef17752db99575f37d3b44aee0189918e3d09b0f46f81ad92f6363bb` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/8FACD8F013577EFBB12547A2F87A551E1/ExplorerFrame.pdb) |
| `10.0.26100.2894` | `81D83040-D08C-49A1-D465-25EFB7709FCA` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/7D07B96A28D000/explorerframe.dll) | `5b624c4f70cc77307e3df8bc78886f58a020fbb4f0c50b5461fc3375636ad877` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/81D83040D08C49A1D46525EFB7709FCA1/ExplorerFrame.pdb) |
| `10.0.26100.3037` | `F1622050-0830-7630-7B8B-8D380C549587` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/A068ED4A292000/explorerframe.dll) | `f205d91fa8180561489554f677e248f7307022b3cf976e7f79f1eb5f87e2060b` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/F1622050083076307B8B8D380C5495871/ExplorerFrame.pdb) |
| `10.0.26100.3323` | `D59DFDC7-94C4-CEB5-3E87-367B410874C2` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/FDDE897C29A000/explorerframe.dll) | `2e6809b602af21817be79b8a9ebc25f6a6e2e5da7149dbb7013a7ad986dca360` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/D59DFDC794C4CEB53E87367B410874C21/ExplorerFrame.pdb) |
| `10.0.26100.3624` | `6A3DAAC9-EE88-C266-867C-DEBB1164F51D` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/013E0B9F2A1000/explorerframe.dll) | `190809e9f98e629b3a81d9ab75ea22aba26c220a21a617187524180ac31a96df` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/6A3DAAC9EE88C266867CDEBB1164F51D1/ExplorerFrame.pdb) |
| `10.0.26100.3912` | `07EF89E6-E05F-4210-B22A-237B4471F1E0` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/4C632FD32B6000/explorerframe.dll) | `521ab01e5130c05c108a8283c5e7871874d20a9e10e7924f11349abb61266343` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/07EF89E6E05F4210B22A237B4471F1E01/ExplorerFrame.pdb) |
| `10.0.26100.4061` | `8CA4DFEC-47E5-CDA1-4CB4-E4E8511CFDB5` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/6E905A1C2B6000/explorerframe.dll) | `36d50b6b2efa84c8abe2d23d6250744784537056b215ee2c0aaf72bf9c82202e` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/8CA4DFEC47E5CDA14CB4E4E8511CFDB51/ExplorerFrame.pdb) |
| `10.0.26100.1150` | `3612DF9E-C90C-DB9A-7E94-A4DD3FFF1381` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/6B0934F4278000/explorerframe.dll) | `29f4ca1e1e17f28da88377936bf52929878ae2207b6f8f4952dee8f18c8f9f77` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/3612DF9EC90CDB9A7E94A4DD3FFF13811/ExplorerFrame.pdb) |
| `10.0.26100.1455` | `75742287-3EBA-42FE-431F-41EF8EFCF141` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/CCC061F2279000/explorerframe.dll) | `2f5564710e57cba1c97ce11d79195cfe4d0b5b62f03563465ba152e4e8f23efa` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/757422873EBA42FE431F41EF8EFCF1411/ExplorerFrame.pdb) |
| `10.0.26100.1591` | `BCB202AE-253F-E88E-3AC7-B4DABD6A2840` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/0E1C52EC288000/explorerframe.dll) | `de5e001704b6c9e5c2b43337dc35f08d9fe7a58a53580ee3feb6f30097add666` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/BCB202AE253FE88E3AC7B4DABD6A28401/ExplorerFrame.pdb) |
| `10.0.26100.1882` | `56F6CEA4-412A-A28D-AF3D-1A60F97A601B` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/C8A895B1288000/explorerframe.dll) | `e69c5699cfa46719db6c421ff55416e21d7d2d0f96908e81b51e1cab0f52d2fa` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/56F6CEA4412AA28DAF3D1A60F97A601B1/ExplorerFrame.pdb) |
| `10.0.26100.2161` | `D03E0FF8-D98E-A150-5EE7-860D9F6A997F` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/27DA6915289000/explorerframe.dll) | `b12e2d1f9ca503071c5b4e0bff50db6ee1fd49f8bf65652706fd52a398d789e8` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/D03E0FF8D98EA1505EE7860D9F6A997F1/ExplorerFrame.pdb) |
| `10.0.26100.2314` | `4EA07794-D106-15D7-F665-B0FD19480FDD` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/377631F1289000/explorerframe.dll) | `134d1d11735d163873fed047168f09c2921a21cdbbd2396246503fa8ec90d5a3` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/4EA07794D10615D7F665B0FD19480FDD1/ExplorerFrame.pdb) |
| `10.0.26100.1000` | `83D07B8B-0415-7FDE-BA8A-4AE8667E9581` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/870AD0FF278000/explorerframe.dll) | `3ae6176c32e43db7ff496b9fe51416b51b9d843050dc4a58000013e4334af8e2` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/83D07B8B04157FDEBA8A4AE8667E95811/ExplorerFrame.pdb) |
| `10.0.26100.712` | `14B05528-EBAF-508C-ADCD-3B2D93361E5E` | 2 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/A0E4689D279000/explorerframe.dll) | `4f65f087dd59492b0a5e269c43cf126837c31306ab4afde0ca5acaf73e15d7e9` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/14B05528EBAF508CADCD3B2D93361E5E1/ExplorerFrame.pdb) |
| `10.0.22621.5983` | `15DA11F4-AC58-2136-0379-7359A5CA49DC` | 5 | Exact-image/PDB; no live acceptance yet | [DLL](https://msdl.microsoft.com/download/symbols/explorerframe.dll/B7661C592C1000/explorerframe.dll) | `225f04151a50bd90c2faf69fdafaabfa9349e59435bf0ea2adc310733c348647` | [PDB](https://msdl.microsoft.com/download/symbols/ExplorerFrame.pdb/15DA11F4AC58213603797359A5CA49DC1/ExplorerFrame.pdb) |

This is not a claim that every Windows 11 component version is supported.

The current catalog contains 54 exact images: 52 indexed candidates and two
independently identified server-returned images. The development inventory covers
92 indexed candidates; the other 40 comprise nine images lacking the required
interfaces, four deliberately excluded accessor contracts, and 27 without
verified exact-image evidence. An index label or failed acquisition is not a map.

The inspected `10.0.22621.317`, `.608`, `.675`, `.755`, `.900`, `.1105` and
`.1344` images have the older `IFileExplorerTabActions` interface, not the thirteen-slot
`IFileExplorerTabActions2` contract. Their exact
symbols expose `AddTab`, `RemoveTab` and `SelectTab`, but not the native tab-drop,
`AddTabAtIndex` or `MoveTabToNewWindow` primitives used by this adapter. Do not
register fabricated thirteen-slot maps for them or silently substitute path
reopening. They remain unsupported under this transfer contract.

The exact `10.0.22000.348` and `10.0.22000.708` images expose neither actions
interface nor the required transfer methods. They also remain unsupported;
an absent lookup result alone is not evidence of missing transfer capability.

Map versions refer to the executable image's fixed version resource. A localized
shell property can instead report a newer MUI/display version: on the tested
host it reports `26100.9949`, but the main DLL's fixed version and complete SHA-256
match the indexed `26100.9549` image. Never use that merged display value to order
private-code compatibility candidates.

A symbol-server response that differs from the requested index hash is not
evidence for the requested version. An independently inspected returned image
may have its own map under its actual fixed version and CodeView identity;
retain the original request's mismatch instead of relabeling it as successful.

## Source provenance

### Acquisition authority

Downloaded Windows DLL samples and matching public PDBs come from the
[Microsoft public symbol server](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/microsoft-public-symbols),
using `https://msdl.microsoft.com/download/symbols/`. These are development
inputs, not DLLs obtained from third-party download mirrors. A candidate version
index helps locate requests; it is not the provider of the DLL/PDB bytes or proof
that a response matches its version label.

The catalog above is also the checked-in source register. Its image-source links
preserve the recorded Microsoft request URLs, including reused verified downloads,
except for the explicitly marked index-derived returned-image request locators.
SHA-256 values identify the actual accepted DLL bytes, not the originally requested
index entry when those differ. PDB links identify the matching symbol files.
These records must remain available in Git independently of ignored research
artifacts; they are evidence, not additional runtime hash-whitelist behavior.

### Installed baseline and returned images

The `10.0.26100.9549` DLL baseline was inspected directly from
`%SystemRoot%\System32\ExplorerFrame.dll` on the development machine, rather than
downloaded for that acquisition. Its catalog checksum identifies those bytes;
Windows servicing may replace the file at the same path. Its PDB locator is
reconstructed from the recorded CodeView GUID/age and is marked identity-derived,
not represented as a retained original download URL. The MUI/display-version
distinction above still applies.

The two returned-image entries retain their original requests:
`10.0.22621.6133` returned the independently inspected `10.0.22621.6197` image,
and `10.0.22621.5840` returned `10.0.22621.5909`. Their source links therefore
describe those requests, not successful acquisition of `.6133` or `.5840`.
Only the actual returned versions, checksums, identities and verified contracts
belong in the supported catalog.

For these two returned images, the original DLL request locators are reconstructed
from the retained index's timestamp/image-size fields using the collector's URL
construction rule and are marked index-derived. Their actual accepted checksums
and matching PDB locators identify the inspected responses separately.

### Locator format and acceptance

The acquisition tooling uses these Microsoft symbol-store locator forms:

```text
https://msdl.microsoft.com/download/symbols/<dll-name>/<timestamp-8-hex><SizeOfImage-hex>/<dll-name>
https://msdl.microsoft.com/download/symbols/<pdb-name>/<CodeView-GUID-without-hyphens><age-hex>/<pdb-name>
```

The DLL key uses the PE `TimeDateStamp` and virtual `SizeOfImage`, not file length
or the human-readable version. The PDB key comes from the DLL's RSDS record;
the current catalog has age 1. See Microsoft's
[PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
for the distinction between file offsets, RVAs and image metadata.
Neither a locator nor a successful HTTP response proves exact content. Compare
the actual fixed version and DLL checksum against the intended sample, then
verify the PDB GUID/age, complete symbol-file length, and exact-PDB probe result.
Changed server responses must pass those checks independently; never rewrite a
requested version's failed provenance to make it appear successful.

### Evidence retention and distribution boundary

For each new or replaced map, update the catalog's identity, source URLs and DLL
checksum in the same change. Retain the following development evidence:

- Requested version, discovery-index URL/revision and expected checksum when used.
- Actual fixed version, recorded download URL or installed source path, verified
  image path, file length, PE image size and actual DLL SHA-256.
- CodeView GUID/age, PDB name and URL, complete PDB length and exact-match result.
- Analysis report, typed entries, function boundaries, accessor rule and the
  distinction between offline checks and scoped live transfer evidence.
- Explicit mismatch, truncation, exclusion and missing-interface outcomes;
  retained failed payloads must not be confused with the verified image path.

Detailed inventories, `provenance.json` files and reports currently live under
`build/research/abi-versions-20261107/`, including `inventory.json` and
`server-returned-inventory.json`. These ignored files are supporting evidence,
not the only source register. The retained `index.json` does not record its
upstream retrieval URL; do not attribute it to an unverified index provider or
treat its labels as authoritative binary provenance. Earlier retained metadata
may use different field names; preserve the meaning rather than infer a source
from a filename alone.
The application ships only compiled adaptation data. Windows DLL samples and
public PDBs stay out of version control and release ZIPs, and the application does
not download them at runtime. The packaged `QuickkyExplorerHook.dll` is QTabs'
own build output; packaged VC++ redistributable DLLs are a separate build/packaging
input described in the overall design, not these ExplorerFrame research samples.

## Versioned maps and reusable rules

- `src/explorer/abi_profiles/profile.h` defines image identity, locator data,
  accessor-rule ID, and a human-readable component version.
- `src/explorer/abi_profiles/<VERSION>.cpp` is the implementation unit for that
  exact component version. It owns its immutable reviewed map:
  CodeView GUID/age, image size, window/tab-actions vtable RVAs, lookup RVA,
  thirteen typed method RVAs, and its accessor-rule ID. Full-entry fingerprints
  and the numeric component version additionally constrain fallback selection.
  Each unit exposes one profile getter. The `9549` unit owns accessor rule 1,
  reused by `9818` and `9278`; the `1` unit owns accessor rule 2, reused by
  `8655`, `8737` and `8972`, and the `28000.3086` unit owns rule 3. The `28000.2605`
  unit owns rule 4, reused by `28000.2525` and `28000.1761`.
  The `22621.5413` unit owns rule 5, reused by the other mapped `22621` images.
  Do not copy the controller,
  protocol, storage checks or tab-operation code into each version unit.
- `src/explorer/native_abi_profiles.cpp` registers the versioned map files.
- `src/explorer/native_abi.cpp` implements common metadata, candidate ordering,
  hashing and storage checks. `native_abi_internal.h` is the private boundary
  between those checks and the version units' accessor recognizers. Maps share
  a recognizer when their accessor code/storage contract is unchanged.

Version labels organize maps, not permission to call an interface. Prefer an
exact CodeView identity/size match and verify its accessor rule. If identity is
unknown, use the actual loaded module's fixed version resource to consider only
same-or-lower numeric versions, nearest first, under the stronger fallback checks
below. There is no download, shifted-RVA search, or unconditional version-range
admission.

## Binding flow

The tray's explicit compatibility check uses the same catalog/contract selector
with `LOAD_LIBRARY_AS_IMAGE_RESOURCE` alone to require an image layout. It checks
the returned handle tag before any RVA reads, rejects raw-data mappings, and uses
the preferred base for an image resource or the actual base when Windows retains
an already-loaded executable module. The reported fixed version always belongs
to the checked mapping, which may precede a pending on-disk update. It releases
its mapping/reference immediately. It does
not execute DLL code, parse symbols, inject, or infer live transfer success.
This diagnostic is separate from target binding, which always checks the actual
module loaded by the selected Explorer thread.

1. On the owning Explorer UI thread, inspect the actual loaded module's bounded
   PE metadata and CodeView identity. Do not base selection on a newer on-disk DLL.
   Require exactly one valid RSDS record with nonzero GUID/age; malformed or
   duplicate records reject the image and leave the output identity empty.
2. Prefer a matching identity in the compiled catalog. Otherwise consider
   same-or-lower versions, nearest first, using the read-only fallback gate. A
   failed candidate must not prevent a later valid candidate from being checked.
3. Reject unknown rule IDs. Validate section bounds, read-only vtables,
   executable method entries, the recognized accessor bodies, and the original
   lookup-call target. Derive object offsets only after these checks succeed.
4. Validate the root's window-data and tab-actions pointers before taking a COM
   reference or reading private storage. Existing identity, lease, folder-scope,
   GUID/PIDL confirmation, and uncertain-transfer quarantine checks still apply.
   Native object/record ranges must fit one committed readable memory region;
   execute-only, guarded, inaccessible, and uncommitted pages are not readable.
5. Unsupported images/rules remain ordinary Explorer windows. No clipping,
   additional Windows 11 strip, path-reopen fallback, or process termination occurs.

All map selection runs locally, with no controller worker or cross-process ABI
descriptor. The existing hook protocol remains unchanged. The version hint comes
only from the loaded module, never a newer on-disk DLL. Runtime PDB parsers are
not part of admission.

## Accessor rule 1

The inspected `IFileExplorerTabActions2` has no initial enumeration or selected-
GUID getter. `RegisterTabEventsHandler` delegates to registration machinery; no
initial replay is established. Events cannot reconstruct a pre-existing source
group. No reviewed public COM/QueryInterface route replaces the initial snapshot.

Rule 1 recognizes complete primary unwind ranges of `GetTabDataById` and
`GetBrowserTabFromId` using reviewed SHA-256 fingerprints. Normalize only known
relative references, three object-access displacement operands, and four reviewed
`Return_Hr` diagnostic immediates at data offsets `0x1ED`, `0x20D`, `0x238`, and
`0x27C`. These diagnostic operands are source-location metadata, not layout or
ownership data. Their surrounding instructions remain fingerprinted. Preserve
the remaining instructions, branches, 40-byte browser records/GUID fields, and
32-byte tab-data output layout. Both output pointers use task-allocator ownership.
The masks are an explicit code-shape contract, not a general x64 byte scanner.

After recognition, derive the actions adjustment, vector begin/end fields, and
selected-browser field. Check alignment, bounded positive offsets, adjacent
vector pointers, non-overlapping active storage, and the actual helper-call
target. No fixed object offsets are copied from the versioned map. Relinking and
field displacement changes can share this rule; an unfamiliar compiler shape
cannot, even if its symbol names remain familiar.

The thirteen-slot interface, native drop entry point 4, private callees,
allocator and drop semantics remain private compatibility contracts. Fingerprints
do not formally prove every transitive behavior. Only `10.0.26100.9549` has scoped
live evidence; offline checks do not establish end-to-end operation on the other
Windows installations. `10.0.26100.9818` and `10.0.26100.9278` share rule 1,
including its reviewed diagnostic operands. The `9278` image derives tabs
`0x1308` and active `0x1320`, unlike the `9549/9818` layout.

## Accessor rule 2

The exact `10.0.26100.1` symbols retain all thirteen ordered signatures. Its
`GetTabDataById` primary range is `0x2F5` bytes instead of `0x2CD`; it obtains the
title through an interface dispatch rather than a direct browser-host call.
`TabDataFree` still frees the outputs at `+0x10/+0x18` using `CoTaskMemFree`, and
the output is still 32 bytes. The lookup body retains the 40-byte record/GUID
contract and shares rule 1's reviewed lookup fingerprint.

Rule 2 has its own complete data-accessor fingerprint and explicit relative,
displacement and diagnostic operands. It derives the actions adjustment from
`+0x4A`, checks the helper call at `+0x60`, and reads the active-field displacement
at `+0xAC`. Its observed layout is actions `0xE8`, tabs `0x198`, active `0x1B0`;
these values are derived from code, not blindly installed from the version label.
The exact `26100.8737` and `26100.8972` images also match rule 2 and retain the
thirteen typed signatures, but derive tabs `0x1308` and active `0x1320` instead.
The exact `26100.8655` image shares this rule with tabs `0x208` and active `0x220`.

## Accessor rule 3

The exact `10.0.28000.3086` image retains all thirteen ordered signatures, the
40-byte record lookup contract and 32-byte output. Its primary data-accessor
extent is `0x2BF`; it uses direct title retrieval and task-allocator ownership.
The root adjustment operand is at `+0x3D`, the lookup-call operand at `+0x4A`
(relative to `+0x4E`), and the active-field operand at `+0x8E`. This different
instruction shape must not be admitted by rule 1 or 2. A complete independent
fingerprint recognizes it before shared storage checks derive actions `0xE8`,
tabs `0x1308` and active `0x1320`. The lookup still recognizes the shared `0xE3`
body and derives root vector fields `0x13F0/0x13F8`.

## Accessor rule 4

The exact `28000.2525` and `28000.2605` images share a `0x2EA` primary data
body and the thirteen-slot signatures. Their title retrieval uses the interface
dispatch at slot `+0x28`, with the same 32-byte output and task-allocator cleanup.
Rule 4 independently recognizes that body, then reads root adjustment `+0x3D`,
lookup rel32 `+0x4A` and active displacement `+0xA2`. The reviewed diagnostic
operands are `0x20A/0x22A/0x255/0x299`.

Despite the shared body, layouts differ: `2525` derives actions/tabs/active
`0xE8/0x258/0x270` from root vector `0x340/0x348`; `2605` derives
`0xE8/0x1308/0x1320` from `0x13F0/0x13F8`. This is why profiles cannot install
another version's object offsets. The recognized code, not revision proximity,
must derive and validate the fields.
The exact `28000.1761` image also matches rule 4; its independently derived
actions/tabs/active layout is `0xE8/0x1A8/0x1C0`, not either newer layout.

## Accessor rule 5: 22621 split lookup

The exact `22621.5413`, `22621.5331` and `22621.5262` images expose the same
thirteen typed Actions2 entries. Their public lookup symbol is spelled without
parameter decoration; development probes must accept that exact bare name as
well as the decorated form. The original probe's missing-locator result was
not missing transfer capability.

Rule 5 recognizes a complete `0x347` data accessor and a complete `0xCF` lookup.
The data root adjustment is at `+0x6E`, lookup-call operand at `+0x86`, and
selected-browser operand at `+0x117`. Diagnostic immediates are
`0x45/0xA3/0x166/0x265`. The output remains 32 bytes with task-allocated title
and PIDL fields at `+0x10/+0x18`.

The lookup derives its vector pointers from `+0x17/+0x21`. It retains the
40-byte record with browser/site pointers, GUID at `+0x10`, and flags at `+0x20`.
Its not-equal branch at `+0x41` reaches a separate nine-byte cold fragment:
`add rbx,0x28; jmp rel32`. Validate its executable/unwind bounds, complete
normalized body and exact backedge to lookup `+0x28`; validating only the hot
range would leave the record stride unproven. These images derive actions
`0xE8`, tabs `0x1A0`, and active `0x1B8`.

The inspected compact accessor layouts in `10.0.22621.2215`, `10.0.22621.2361`,
`10.0.22621.2792` and `10.0.22621.3085` are intentionally outside the supported
catalog. Their transfer interfaces exist, but their data/lookup contracts do not
match rule 5. Do not admit them by version proximity or reuse offsets from a
different shape.

## Conservative nearest-version fallback

An unknown CodeView identity may use a candidate only after read-only validation:

1. Read the numeric four-word version from the loaded module's fixed version
   resource. Missing/malformed version metadata disables fallback, not exact
   identity matching. Never read a replacement DLL from disk for this hint.
2. Consider only same-or-lower catalog versions, nearest first. Do not try newer
   versions or mutate Explorer while testing a candidate.
3. Require the candidate's original vtable/method RVAs and all thirteen complete
   entry bodies to match their stored fingerprints. Require recorded unwind
   boundaries for non-leaf entries; reviewed leaf thunks include their complete
   jump/return. Only the reviewed data-accessor diagnostic immediates may differ
   in this extra check: do not wildcard relative references or object offsets.
4. Rebind the descriptor to the inspected identity only after the entry check;
   then validate its accessor rule, lookup target, sections and derived fields.
   A failed candidate permits checking the next lower candidate.
5. Return no adapter if no candidate passes. File-version proximity alone never
   authorizes a private call. Moved functions or unfamiliar bodies need a new
   development-time map; this is not universal forward compatibility.

Fallback recognizes an already reviewed code contract, not an arbitrary private
ABI from names alone. Transitive private behavior remains a compatibility risk;
keep destination confirmation, source revalidation and timeout quarantine.

## Adding support for another component version

1. Obtain the exact image and matching public PDB in a development environment.
   Record file version, CodeView GUID/age, image size, and symbol provenance.
   Use the recorded verified image path: a rejected or truncated payload may be
   retained separately. Recheck provenance before generating a map.
2. Inspect the window/tab-actions vtables and all thirteen method signatures,
   their order/extent, `this` adjustments, snapshot ownership, and native drop
   behavior. A function name alone is insufficient evidence.
3. If the accessor contract matches an existing rule, add only
   `abi_profiles/<VERSION>.cpp` and register it in `native_abi_profiles.cpp`, including
   exact full-entry fingerprint lengths/boundaries and the numeric version.
   Record every RVA from that exact image; do not shift an older map by a delta.
4. If code shape changes, add a reviewed rule with its own full boundaries,
   normalization operands, derivation, and negative tests. If record layout,
   signature, extent, or ownership changes, update the adapter contract first.
   Do not broaden an existing rule with wildcard hashes or version-range bypasses.
5. Run C++17 builds and CTest, including catalog selection, unknown-map/rule
   rejection, damaged accessor, relocation, and field-derivation cases. Validate
   actual add/select/remove, current-folder `+`, pre-existing multi-tab sources,
   and maximized windows using the directory-scoped
   [Explorer testing procedure](../guides/EXPLORER_TESTING.md).
   `QuickkyNativeAbiTests.exe --image PATH\ExplorerFrame.dll` additionally checks
   an explicitly supplied reviewed image: its exact map, derived layout,
   fallback rejection/selection, each typed-entry fingerprint, fixed version
   resource and ASLR relocation. The test process does not initialize that DLL,
   invoke its private interfaces or attach to Explorer. Downloaded images/PDBs
   stay in ignored research output, not the product package or source tree.
6. Claim compatibility only for the images and routes actually tested. Preserve
   destination GUID/PIDL confirmation and source revalidation in every rule.
