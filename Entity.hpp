//Entity.h

#include <SFML/Graphics.hpp> //for sf::Sprite

class Entity {
public:
	int id = -1;
	sf::Sprite* sprite;
	bool deleteThis = false;
	virtual void Update(){
		//pass
	};
	
	virtual ~Entity(){
		delete sprite;
	}
};

/*
example of an override
class PlayerShot : public Entity {
	float speed = 0.2f;
	void Update() override {
		sprite.move(0.0f, -speed);
		if (sprite.getPosition().y < -100) deleteThis = true;
	}
};
*/