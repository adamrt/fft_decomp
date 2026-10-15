// twins.go groups functions across modules whose instruction streams match
// once every call and address operand is read as a symbol name, under a
// consistent one-to-one renaming. See twinsCommand.
package main

import "errors"

// twinTrials builds the trials that test a twins-report group both ways.
func (p project) twinTrials(config *projectConfig, report string, ids []string) ([]trialSpec, error) {
	return nil, errors.New("twins report trials are not available yet")
}

func (p project) twinsCommand(args []string) error {
	return errors.New("twins is not available yet")
}
