//DrawQ.cpp
//used to keep track of all Drawables, and their sorting order. simplifies/automates drawing & layering of entities.
#include <vector>

namespace DrawLayers{
	const int NUM_LAYERS = 5;
	enum Layer {
		background, backstage, stage, foreground, HUD
	};
}

class DrawQ{
	private:
	struct entry {
		int id;
		DrawLayers::Layer layer;
		int order;
		sf::Drawable* drawable = NULL;
		entry* next = NULL;
	};
	entry* layerHeads[DrawLayers::NUM_LAYERS] = {NULL};
	
	public:
	void add(sf::Drawable&, DrawLayers::Layer, int, int);
	void remove(int);
	void changeOrder(int, int);
	void changeLayer(int, DrawLayers::Layer);
	void drawToWindow(sf::RenderWindow&);
};


void DrawQ::add(sf::Drawable &drawable, DrawLayers::Layer layer, int id, int order=0){
	entry* newEntry = new entry;
	newEntry->layer = layer;
	newEntry->order = order;
	newEntry->drawable = &drawable;
	newEntry->id = id;
	
	entry* currentEntry;
	entry* prevEntry = NULL;
	
	//if this is the first entry added, make it head
	if (layerHeads[layer] == NULL){
		layerHeads[layer] = newEntry;
		return;
	}
	
	//if we should become the new head, based on sorting order
	if (layerHeads[layer]->order > order){
		newEntry->next = layerHeads[layer];
		layerHeads[layer] = newEntry;
		return;
	}
	
	//find a place in the middle or at the end of the queue
	prevEntry = layerHeads[layer];
	currentEntry = prevEntry->next;
	while(currentEntry != NULL){
		if (currentEntry->order > order){
			//insert new entry at this position
			break;
		}
		prevEntry = currentEntry;
		currentEntry = currentEntry->next;
	}
	newEntry->next = currentEntry;
	prevEntry->next = newEntry;
}

void DrawQ::remove(int id){
	//table walk through each layer... not very sophisticated, but this is an action that shouldn't frequently occur (on a frame-frame scale).
	for (int l=0; l < DrawLayers::NUM_LAYERS; l++){
		entry* prevEntry = NULL;
		entry* currentEntry = layerHeads[l];
		
		if (currentEntry == NULL) return; //there is nothing to remove or search through
		
		//if we are to remove the head, update the layer head.
		if (currentEntry->id == id) {
			layerHeads[l] = currentEntry->next;
			delete(currentEntry);
			break;
		}
		//else, search through this layer for the id.
		else{
			prevEntry = currentEntry;
			currentEntry = currentEntry->next;
			while (currentEntry != NULL && currentEntry->id != id){
				prevEntry = currentEntry;
				currentEntry = currentEntry->next;
			}
			
			if (currentEntry != NULL && currentEntry->id == id){
				prevEntry->next = currentEntry->next;
				delete(currentEntry);
				break;
			}
		}
	}
}

void DrawQ::changeOrder(int id, int newOrder){
	//table walk through each layer..... not very sophisticated, but this is an action that shouldn't frequently occur (on a frame-frame scale).
	//has to search for two things: the id'd entry to move, and the new place to put the entry.
	//walks each layer once until both things are found.
	
	
	//for each layer:
	for (int l=0; l < DrawLayers::NUM_LAYERS; l++){
		entry* prevEntry = NULL;
		entry* currentEntry = layerHeads[l];
		entry* newPrevEntry = NULL; //bookmarks a located entry that we ought to move the id'd entry to be after (if the id'd entry is to be moved to head, this is not used)
		entry* toMoveEntry = NULL; //the entry to move. if found before a location is found, continue searching.
		bool movingHead = false; //note for if we are moving the head (the head is id'd). mutually exclusive with isNewHead, unless we are moving the head to its current position.
		bool isNewHead = false; //note for if we are to become the new head
		
		if (currentEntry == NULL) return; //there is nothing to remove or search through
		
		//if the head is id'd, take a note of it.
		if (currentEntry->id == id) {
			toMoveEntry = layerHeads[l];
			movingHead = true;
		}
		
		//if we are to move the entry to be the head, take a note of it.
		if (newOrder < layerHeads[l]->order){
			isNewHead = true;
		}
		
		//search as long as we: (have not determined where we are moving the entry) or (have not determined which entry to move).
		while (currentEntry != NULL && (newPrevEntry == NULL && isNewHead == false) || (movingHead == false && toMoveEntry == NULL)){
			prevEntry = currentEntry;
			currentEntry = currentEntry->next;
			
			//check if this entry is the entry to move
			if (currentEntry != NULL && currentEntry->id == id){
				toMoveEntry = currentEntry;
			}
			//check if the entry should be placed here
			if (newOrder < currentEntry->order){
				newPrevEntry = prevEntry;
			}
		}
		
		//if we have enough information to execute the move after searching this layer:
		if ((newPrevEntry != NULL || isNewHead == true) && (movingHead == true || toMoveEntry != NULL)){
			//place the entry where it should be. procedure changes if it was the head or is the new head.
			if (movingHead && isNewHead){
				layerHeads[l]->order = newOrder;
			}
			else if (movingHead){
				layerHeads[l] = toMoveEntry->next;
				toMoveEntry->next = newPrevEntry->next;
				newPrevEntry->next = toMoveEntry;
			}
			else if (isNewHead){
				toMoveEntry->next = layerHeads[l];
			}
			else{
				toMoveEntry->next = newPrevEntry->next;
				newPrevEntry->next = toMoveEntry;
			}
			//do not move on to the next layer.
			break;
		}
	}
}

void DrawQ::changeLayer(int id, DrawLayers::Layer newLayer){
	//TODO: remove() from current layer, add() to new layer
}

void DrawQ::drawToWindow(sf::RenderWindow& window){
	for (int l=0; l<DrawLayers::NUM_LAYERS; l++){
		//for each layer, starting with the back and heading forward
		entry* currentEntry = layerHeads[l];
		while(currentEntry != NULL){
			window.draw(*(currentEntry->drawable)); //this is ugly but it works
			currentEntry = currentEntry->next;
		}		
	}
}