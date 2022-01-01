![Imgur](https://imgur.com/oXGegKe.png)
# Tasty Haste Plugin - UE4 Editor Mode for Rapid Mesh Placement

**Tasty Haste** is a Custom Unreal Engine 4 Editor Mode that allows you to place, scale, and rotate repeated meshes with a single mouse click, and without having to drag and drop every mesh/actor.  

## Usage
1. Enter **Haste Edit Mode** by selecting the Haste tab in the editor 
![Imgur](http://i.imgur.com/QfR5pNH.png)

2. Select a mesh from the content browser that you would like to place on the map
![Imgur](http://i.imgur.com/QZiyLC9.png)

3. Hover the mouse over the viewport and click to place the mesh
![Imgur](http://i.imgur.com/cpbft6Y.png)

4. Hold down shift and use the mouse wheel to rotate the mesh cursor

5. Hold the left mouse button while placing a mesh and drag to resize the mesh in the XY axis.

6. Use the mouse wheel to scale the mesh in the Z direction.

## Installation
* Create a folder named Plugins in your UE4 game root directory
* Extract the contents of this repository into a directory named Haste
* The final directory structure should look like this

![Imgur](http://i.imgur.com/SWjVGg2.png)


### Example
<pre>
cd ShooterGame
mkdir Plugins
cd Plugins
git clone https://github.com/siditious/haste-plugin-ue4.git Haste
</pre>

This would create a folder named Haste inside the Plugins folder and clone the repository into it

### Example (if your main game code is already in git)
You can clone the plugin into your game's existing git repository as a sub module
<pre>
cd ShooterGame
mkdir Plugins
git submodule add https://github.com/siditious/haste-plugin-ue4.git Plugins/Haste
</pre>

Tasty Haste Camera controls:
  W,S - move forward/backward in the XY plane
  A,D - move along the left/right vector in the XY plane relative to the camera forward.
  Q,E - move up/down in the Z plane.
  
  Camera speed is scaled with editor viewport setting.
  
  camera rotation locks to viewport rotation grid, which is highly ideal for RPG creation and isometric maps.  
  
 Currently only tested on UE5 EA2, but should work in 4.26/4.27
