import subprocess

def run(cmd):
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

cmd_out = subprocess.check_output('docker ps -a', shell=True, text=True).split('\n')
cmd_out = cmd_out[0:len(cmd_out)-1]

if len(cmd_out) == 1:
    print('Creating broker container')
    subprocess.run('docker compose create', shell=True, cwd='./server/')
    cmd_out = subprocess.check_output('docker ps -a', shell=True, text=True).split('\n')
    cmd_out = cmd_out[0:len(cmd_out)-1]

categories = cmd_out[0]
containers = cmd_out[1:]

container_statuses = {}

for c in containers:
    index = categories.index('STATUS')
    temp = c[index:].split()
    container_statuses[temp[len(temp)-1]] = temp[0]

if container_statuses['broker'] != 'Up':
    print('Running broker container')
    run('docker start broker')

subprocess.run('go run .', shell=True)

print('Killing and deleting container')
run('docker kill broker')
run('docker rm broker')
