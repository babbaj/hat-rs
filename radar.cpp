#include <SFML/Graphics.hpp>

#include "utils.h"

sf::Vector2f center(const sf::RenderWindow& window) {
    const auto [lenX, lenY] = window.getSize();
    return {lenX / 2.f, lenY / 2.f};
}

void centerOrigin(sf::CircleShape& circle) {
    circle.setOrigin( circle.getRadius(), circle.getRadius());
}

void drawPlayer(sf::RenderWindow& window, const sf::Vector2f& pos) {
    sf::CircleShape playerCircle(10);
    centerOrigin(playerCircle);
    playerCircle.setPosition(pos.x, pos.y);
    playerCircle.setFillColor({0, 0, 255});

    window.draw(playerCircle);
}



sf::Vector2f mult(const sf::Vector2f& vec, float operand) {
    return {vec.x * operand, vec.y * operand};
}



void runRadar(WinProcess& rust) {
    sf::RenderWindow window(sf::VideoMode(900, 900), "radar");

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event)) if (event.type == sf::Event::Closed) window.close();
        window.clear(sf::Color{255, 255, 255});

        constexpr float SCALE = 4.5; // 4.5 pixels per meter

        const auto [centerX, centerY] = center(window);
        drawPlayer(window, center(window));

        const player local = player{rust, getLocalPlayer(rust)};
        const std::vector players = getVisiblePlayers(rust);
        const float yaw = local.angles.x;

        for (const auto& player : players) {
            if (player.handle.address == local.handle.address) continue;
            const auto me = sf::Vector2f{local.position.x, local.position.z};
            const auto them = sf::Vector2f{player.position.x, player.position.z};

            const auto relativeRadarPos = (me - them) * SCALE;
            sf::CircleShape enemyCircle(4.5);

            enemyCircle.setFillColor({255, 0, 0});
            enemyCircle.setPosition(center(window) + relativeRadarPos);
            // TODO: rotate
            window.draw(enemyCircle);
        }

        window.display();
    }
}