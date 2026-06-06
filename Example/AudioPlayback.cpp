#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

int main() {
	// Create the main window
	sf::RenderWindow window(sf::VideoMode({1920, 1080}), "SFML window");

	// Load a sprite to display
	const sf::Texture texture("Media/Images/Backgrounds/start_background.png");
	sf::Sprite sprite(texture);

	// Load a music to play
	sf::Music music("Media/Audios/Music/seibin,shift up - Dawn.mp3");

	// Play the music
	music.play();

	// Start the game loop
	while (window.isOpen())
	{
		// Process events
		while (const std::optional event = window.pollEvent())
		{
			// Close window: exit
			if (event->is<sf::Event::Closed>())
				window.close();
		}

		// Clear screen
		window.clear();

		// Draw the sprite
		window.draw(sprite);

		// Update the window
		window.display();
	}
}