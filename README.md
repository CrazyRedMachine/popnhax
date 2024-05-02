[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/donate?hosted_button_id=WT735CX4UMZ9U)

# popnhax

Patcher for pop'n music arcade game.

Featuring pfree, instant retire, visual and audio offset adjust, 1000Hz input polling for true ms-based timing, unlimited favorites, auto hi-speed selection, iidx-like hard gauge and more..

Based on [bemanihax](https://github.com/windyfairy/bemanihax) whose an updated version was included with omnimix v1

### Features

Refer to [popnhax.xml](https://github.com/CrazyRedMachine/popnhax/blob/main/dist/popnhax/popnhax.xml) for complete list and explanations

More info could be found in [Release Notes](https://github.com/CrazyRedMachine/popnhax/releases) or in the [popnhax_tools](https://github.com/CrazyRedMachine/popnhax_tools) repository.

### Run Instructions

- Extract all files directly in the `contents` folder of your install.

(**Note**: if you're running your dlls from `modules` subfolder, please rather copy them back into `contents` folder).

- Edit `popnhax.xml` with a text editor and set your desired options.

- Add `popnhax.dll` as an inject dll to your gamestart command or option menu.

eg. modify your gamestart.bat to add `-k popnhax.dll` or `-K popnhax.dll` depending on the launcher you use.

Some launchers also feature an option menu (accessible by pressing F4 ingame), in which case you can locate the "Inject Hook" setting in option tab. Enter `popnhax.dll` there.

### Build Instructions

Using WSL is the recommended method. Just run `make`.
