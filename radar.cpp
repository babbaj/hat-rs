#include <cmath>

#include <SFML/Graphics.hpp>

#include "utils.h"

sf::Vector2f center(const sf::RenderWindow& window) {
    const auto [lenX, lenY] = window.getSize();
    return {lenX / 2.f, lenY / 2.f};
}

void centerOrigin(sf::CircleShape& circle) {
    circle.setOrigin( circle.getRadius(), circle.getRadius());
}

void drawSelf(sf::RenderWindow& window, const sf::Vector2f& pos, float yaw) {
    sf::CircleShape playerCircle(10);
    centerOrigin(playerCircle);
    playerCircle.setPosition(pos.x, pos.y);
    playerCircle.setFillColor({0, 0, 255});

    const auto wCenter = center(window);
    sf::Vertex line[2];
    line[0].position = wCenter; line[0].color = {0, 0, 0};
    line[1].position = wCenter; line[1].color = {0, 0, 0};
    line[1].position.y -= 15;
    sf::Transform lineRotate;
    lineRotate.rotate(yaw, wCenter);
    line[1].position = lineRotate.transformPoint(line[1].position);


    window.draw(playerCircle);
    window.draw(line, 2, sf::PrimitiveType::LineStrip);
}

void drawEnemy(sf::RenderWindow& window, const player& player, const sf::Vector2f& pos) {
    sf::CircleShape enemyCircle(4.5);
    centerOrigin(enemyCircle);
    enemyCircle.setPosition(pos);
    enemyCircle.setFillColor({255, 0, 0});

    window.draw(enemyCircle);
    const std::string& name = player.name;
    std::string health = std::to_string((int)std::round(player.health)) + "hp";
    std::string yStr = "y" + std::to_string((int)std::round(player.position.y));
    // TODO: get held weapon name

    static const auto font = [] {
       sf::Font out;
       if (!out.loadFromFile("arial.ttf")) {
           throw "failed to open font";
       }
       return out;
    }();
    float offset = enemyCircle.getRadius() + 2;
    for (const std::string& str : {yStr, health, name}) {
        sf::Text text{str, font, 10};
        text.setFillColor(sf::Color::Black);

        const auto height = text.getLocalBounds().height;
        sf::Vector2f textPos = pos;
        offset += height + 2;
        textPos.y -= offset;
        text.setPosition(textPos);
        window.draw(text);
    }
}

void runRadar(WinProcess& rust) {
    sf::RenderWindow window(sf::VideoMode(900, 900), "radar");

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event)) if (event.type == sf::Event::Closed) window.close();
        window.clear(sf::Color{255, 255, 255});

        constexpr float SCALE = 4.5; // 4.5 pixels per meter

        const player local = player{rust, getLocalPlayer(rust)};
        const std::vector players = getVisiblePlayers(rust);

        const float yaw = local.angles.y;
        drawSelf(window, center(window), yaw);

        for (const auto& player : players) {
            if (player.handle.address == local.handle.address) continue;
            const auto me = sf::Vector2f{local.position.x, local.position.z};
            const auto them = sf::Vector2f{player.position.x, player.position.z};

            auto relativeRadarPos = (them - me) * SCALE;
            relativeRadarPos.y *= -1;

            drawEnemy(window, player, center(window) + relativeRadarPos);
        }

        window.display();
    }
}