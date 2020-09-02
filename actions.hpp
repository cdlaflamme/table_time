//states.hpp
//outlines state change notification structures/values

enum ActionType {
	create,		//object should be instantiated
	destroy,	//object should be deleted
	enter_hand,		//object enters an unowned hand zone
	become_secret,	//object enters an unowned secret zone
	roll,			//object is a die and has been rolled
	shuffle,		//object is a stack and it has been shuffled
	pop,			//object is a stack/pile and the top item has been removed.
	push,			//object is a stack/pile and an item has been added on top.
	flip,			//object should reveal its other face.
	move			//object should change position.
}

/*
extra values explanation:
depending on the action, and whether this action is sent by the client or server,
extra information may be needed.
extra information for each valid action type is described below.

create:
	the entire entity dictionary entry of the new object
destroy:
	none (object id included already)
enter_hand:
	the number of the player that owns the hand that is being entered
become secret:
	the number of the player that owns the secret zone that is being entered
roll:
	if from server, contains the roll result as an index (indexes list of faces contained in die object)
shuffle:
	if from server, contains the new id list in order
pop:
	none
push:
	the id of the object that is being placed on top. the bottom object is being pushed into, and is the main action object_id
flip:
	the new face index to be shown
move:
	new x, new y
*/

struct Action {
	int object_id;		//the object id the action is performed on
	int action;			//the action to perform
	int player;			//the performing player
	int* values; 		//optional extra values
	int num_values;		//number of optional extra values (length of array)
	//TODO add entity pointer for object creation? depends on how dictionary is structured. set it to = NULL by default
};