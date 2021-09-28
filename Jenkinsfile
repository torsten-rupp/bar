#!groovy

//TODO: store docker image in registry?
//dockerImageName="dockerregistry.macio.de:5000/macio_backup_archiver"
dockerImageName='macio_backup_archiver'

// build
def build()
{
  try
  {
    stage("Docker image")
    {
      checkout scm
      sh "(cd docker; docker build -t ${dockerImageName} .)"
    }


    stage("Build")
    {
      docker.image(dockerImageName).inside
      {
        checkout scm
        sh "./download-third-party-packages.sh"
        sh "./configure"
//TODO
//        sh "make -j`nproc --ignore=1`"
        sh "make"
        sh "make dist"
      }
    }

    stage("Packages")
    {
      docker.image(dockerImageName).inside
      {
//TODO: how to run a docker from inside a docker?
//        sh "make update_docker"
//        sh "make rpm"
//        sh "make deb"
        sh "make gui"
      }
    }

    stage("Tests")
    {
      docker.image(dockerImageName).inside
      {
//TODO
        sh "make test1"
      }
    }
  }
  finally
  {
    archiveArtifacts artifacts: 'backup-archiver*.tar.bz2, *.rpm, *.deb, *.zip', allowEmptyArchive: 'true'
    cleanWs notFailBuild: true
  }
}

// main
properties([gitLabConnection('GitLab-DMZ')])
try
{
  node('docker')
  {
    build()
  }
}
catch (error)
{
  echo "ERROR: ${error.message}"
  currentBuild.result = 'FAILED'
}
