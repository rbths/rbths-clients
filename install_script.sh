sudo curl -s -L -0 https://github.com/rbths/rbths-clients/raw/main/rbths.gpg | sudo tee /etc/apt/keyrings/rbths.gpg > /dev/null
echo "deb [signed-by=/etc/apt/keyrings/rbths.gpg] https://us-central1-apt.pkg.dev/projects/rbths-01 rbths-apt main" | sudo tee /etc/apt/sources.list.d/rbths.list
sudo apt update