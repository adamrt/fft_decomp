package main

import (
	"errors"
	"os"
	"path/filepath"
	"syscall"
)

// fileLock takes an exclusive advisory lock on path until the returned
// function runs.
func fileLock(path string) (func() error, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return nil, err
	}
	file, err := os.OpenFile(path, os.O_CREATE|os.O_RDWR, 0o644)
	if err != nil {
		return nil, err
	}
	if err := syscall.Flock(int(file.Fd()), syscall.LOCK_EX); err != nil {
		file.Close()
		return nil, err
	}
	return func() error { return errors.Join(syscall.Flock(int(file.Fd()), syscall.LOCK_UN), file.Close()) }, nil
}

// atomicWrite replaces path so readers never see a partial file.
func atomicWrite(path string, data []byte) (err error) {
	file, err := os.CreateTemp(filepath.Dir(path), ".write-*")
	if err != nil {
		return err
	}
	defer func() { file.Close(); os.Remove(file.Name()) }()
	if err = file.Chmod(0o644); err != nil {
		return err
	}
	if _, err = file.Write(data); err != nil {
		return err
	}
	if err = file.Sync(); err != nil {
		return err
	}
	if err = file.Close(); err != nil {
		return err
	}
	return os.Rename(file.Name(), path)
}

// withTargetLock serializes tools that rewrite target/*.yaml.
func (p project) withTargetLock(action func() error) (err error) {
	unlock, err := fileLock(filepath.Join(p.root, "build", "target.lock"))
	if err != nil {
		return err
	}
	defer func() { err = errors.Join(err, unlock()) }()
	return action()
}
